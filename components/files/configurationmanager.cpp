#include "configurationmanager.hpp"

#include <components/debug/debuglog.hpp>
#include <components/files/configfileparser.hpp>
#include <components/fallback/validate.hpp>

#include <boost/filesystem/fstream.hpp>
/**
 * \namespace Files
 */
namespace Files
{

static const char* const openmwCfgFile = "openmw.cfg";

#if defined(_WIN32) || defined(__WINDOWS__)
static const char* const applicationName = "OpenMW";
#else
static const char* const applicationName = "openmw";
#endif

const char* const localToken = "?local?";
const char* const userDataToken = "?userdata?";
const char* const globalToken = "?global?";

ConfigurationManager::ConfigurationManager(bool silent)
    : mFixedPath(applicationName)
    , mSilent(silent)
{
    setupTokensMapping();

    boost::filesystem::create_directories(mFixedPath.getUserConfigPath());
    boost::filesystem::create_directories(mFixedPath.getUserDataPath());

    mLogPath = mFixedPath.getUserConfigPath();

    mScreenshotPath = mFixedPath.getUserDataPath() / "screenshots";

    // probably not necessary but validate the creation of the screenshots directory and fallback to the original behavior if it fails
    boost::system::error_code dirErr;
    if (!boost::filesystem::create_directories(mScreenshotPath, dirErr) && !boost::filesystem::is_directory(mScreenshotPath)) {
        mScreenshotPath = mFixedPath.getUserDataPath();
    }
}

ConfigurationManager::~ConfigurationManager()
{
}

void ConfigurationManager::setupTokensMapping()
{
    mTokensMapping.insert(std::make_pair(localToken, &FixedPath<>::getLocalPath));
    mTokensMapping.insert(std::make_pair(userDataToken, &FixedPath<>::getUserDataPath));
    mTokensMapping.insert(std::make_pair(globalToken, &FixedPath<>::getGlobalDataPath));
}

void ConfigurationManager::readConfiguration(boost::program_options::variables_map& variables,
    boost::program_options::options_description& description, bool quiet)
{
    bool silent = mSilent;
    mSilent = quiet;
    
    // User config has the highest priority.
    auto composingVariables = separateComposingVariables(variables, description);
    loadConfig(mFixedPath.getUserConfigPath(), variables, description);
    mergeComposingVariables(variables, composingVariables, description);
    boost::program_options::notify(variables);

    // read either local or global config depending on type of installation
    composingVariables = separateComposingVariables(variables, description);
    bool loaded = loadConfig(mFixedPath.getLocalPath(), variables, description);
    mergeComposingVariables(variables, composingVariables, description);
    boost::program_options::notify(variables);
    if (!loaded)
    {
        composingVariables = separateComposingVariables(variables, description);
        loadConfig(mFixedPath.getGlobalConfigPath(), variables, description);
        mergeComposingVariables(variables, composingVariables, description);
        boost::program_options::notify(variables);
    }

    mSilent = silent;
}

boost::program_options::variables_map separateComposingVariables(boost::program_options::variables_map & variables,
    boost::program_options::options_description& description)
{
    boost::program_options::variables_map composingVariables;
    for (auto itr = variables.begin(); itr != variables.end();)
    {
        if (description.find(itr->first, false).semantic()->is_composing())
        {
            composingVariables.emplace(*itr);
            itr = variables.erase(itr);
        }
        else
            ++itr;
    }
    return composingVariables;
}

void mergeComposingVariables(boost::program_options::variables_map& first, boost::program_options::variables_map& second,
    boost::program_options::options_description& description)
{
    // There are a few places this assumes all variables are present in second, but it's never crashed in the wild, so it looks like that's guaranteed.
    std::set<std::string> replacedVariables;
    if (description.find_nothrow("replace", false))
    {
        auto replace = second["replace"];
        if (!replace.defaulted() && !replace.empty())
        {
            std::vector<std::string> replaceVector = replace.as<std::vector<std::string>>();
            replacedVariables.insert(replaceVector.begin(), replaceVector.end());
        }
    }
    for (const auto& option : description.options())
    {
        if (option->semantic()->is_composing())
        {
            std::string name = option->canonical_display_name();

            auto firstPosition = first.find(name);
            if (firstPosition == first.end())
            {
                first.emplace(name, second[name]);
                continue;
            }

            if (replacedVariables.count(name))
            {
                firstPosition->second = second[name];
                continue;
            }

            if (second[name].defaulted() || second[name].empty())
                continue;

            boost::any& firstValue = firstPosition->second.value();
            const boost::any& secondValue = second[name].value();
            
            if (firstValue.type() == typeid(Files::MaybeQuotedPathContainer))
            {
                auto& firstPathContainer = boost::any_cast<Files::MaybeQuotedPathContainer&>(firstValue);
                const auto& secondPathContainer = boost::any_cast<const Files::MaybeQuotedPathContainer&>(secondValue);

                firstPathContainer.insert(firstPathContainer.end(), secondPathContainer.begin(), secondPathContainer.end());
            }
            else if (firstValue.type() == typeid(std::vector<std::string>))
            {
                auto& firstVector = boost::any_cast<std::vector<std::string>&>(firstValue);
                const auto& secondVector = boost::any_cast<const std::vector<std::string>&>(secondValue);

                firstVector.insert(firstVector.end(), secondVector.begin(), secondVector.end());
            }
            else if (firstValue.type() == typeid(Fallback::FallbackMap))
            {
                auto& firstMap = boost::any_cast<Fallback::FallbackMap&>(firstValue);
                const auto& secondMap = boost::any_cast<const Fallback::FallbackMap&>(secondValue);

                std::map<std::string, std::string> tempMap(secondMap.mMap);
                tempMap.merge(firstMap.mMap);
                firstMap.mMap.swap(tempMap);
            }
            else
                Log(Debug::Error) << "Unexpected composing variable type. Curse boost and their blasted arcane templates.";
        }
    }

}

void ConfigurationManager::processPaths(Files::PathContainer& dataDirs, bool create)
{
    std::string path;
    for (Files::PathContainer::iterator it = dataDirs.begin(); it != dataDirs.end(); ++it)
    {
        path = it->string();

        // Check if path contains a token
        if (!path.empty() && *path.begin() == '?')
        {
            std::string::size_type pos = path.find('?', 1);
            if (pos != std::string::npos && pos != 0)
            {
                TokensMappingContainer::iterator tokenIt = mTokensMapping.find(path.substr(0, pos + 1));
                if (tokenIt != mTokensMapping.end())
                {
                    boost::filesystem::path tempPath(((mFixedPath).*(tokenIt->second))());
                    if (pos < path.length() - 1)
                    {
                        // There is something after the token, so we should
                        // append it to the path
                        tempPath /= path.substr(pos + 1, path.length() - pos);
                    }

                    *it = tempPath;
                }
                else
                {
                    // Clean invalid / unknown token, it will be removed outside the loop
                    (*it).clear();
                }
            }
        }

        if (!boost::filesystem::is_directory(*it))
        {
            if (create)
            {
                try
                {
                    boost::filesystem::create_directories (*it);
                }
                catch (...) {}

                if (boost::filesystem::is_directory(*it))
                    continue;
            }

            (*it).clear();
        }
    }

    dataDirs.erase(std::remove_if(dataDirs.begin(), dataDirs.end(),
        std::bind(&boost::filesystem::path::empty, std::placeholders::_1)), dataDirs.end());
}

bool ConfigurationManager::loadConfig(const boost::filesystem::path& path,
    boost::program_options::variables_map& variables,
    boost::program_options::options_description& description)
{
    boost::filesystem::path cfgFile(path);
    cfgFile /= std::string(openmwCfgFile);
    if (boost::filesystem::is_regular_file(cfgFile))
    {
        if (!mSilent)
            Log(Debug::Info) << "Loading config file: " << cfgFile.string();

        boost::filesystem::ifstream configFileStream(cfgFile);

        if (configFileStream.is_open())
        {
            parseConfig(configFileStream, variables, description);

            return true;
        }
        else
        {
            if (!mSilent)
                Log(Debug::Error) << "Loading failed.";

            return false;
        }
    }
    return false;
}

const boost::filesystem::path& ConfigurationManager::getGlobalPath() const
{
    return mFixedPath.getGlobalConfigPath();
}

const boost::filesystem::path& ConfigurationManager::getUserConfigPath() const
{
    return mFixedPath.getUserConfigPath();
}

const boost::filesystem::path& ConfigurationManager::getUserDataPath() const
{
    return mFixedPath.getUserDataPath();
}

const boost::filesystem::path& ConfigurationManager::getLocalPath() const
{
    return mFixedPath.getLocalPath();
}

const boost::filesystem::path& ConfigurationManager::getGlobalDataPath() const
{
    return mFixedPath.getGlobalDataPath();
}

const boost::filesystem::path& ConfigurationManager::getCachePath() const
{
    return mFixedPath.getCachePath();
}

const boost::filesystem::path& ConfigurationManager::getInstallPath() const
{
    return mFixedPath.getInstallPath();
}

const boost::filesystem::path& ConfigurationManager::getLogPath() const
{
    return mLogPath;
}

const boost::filesystem::path& ConfigurationManager::getScreenshotPath() const
{
    return mScreenshotPath;
}

void parseArgs(int argc, const char* const argv[], boost::program_options::variables_map& variables,
    boost::program_options::options_description& description)
{
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv).options(description).allow_unregistered().run(),
        variables
    );
}

void parseConfig(std::istream& stream, boost::program_options::variables_map& variables,
    boost::program_options::options_description& description)
{
    boost::program_options::store(
        Files::parse_config_file(stream, description, true),
        variables
    );
}

std::istream& operator>> (std::istream& istream, MaybeQuotedPath& MaybeQuotedPath)
{
    // If the stream starts with a double quote, read from stream using boost::filesystem::path rules, then discard anything remaining.
    // This prevents boost::program_options getting upset that we've not consumed the whole stream.
    // If it doesn't start with a double quote, read the whole thing verbatim
    if (istream.peek() == '"')
    {
        istream >> static_cast<boost::filesystem::path&>(MaybeQuotedPath);
        if (istream && !istream.eof() && istream.peek() != EOF)
        {
            std::string remainder{std::istreambuf_iterator(istream), {}};
            Log(Debug::Warning) << "Trailing data in path setting. Used '" << MaybeQuotedPath.string() << "' but '" << remainder << "' remained";
        }
    }
    else
    {
        std::string intermediate{std::istreambuf_iterator(istream), {}};
        static_cast<boost::filesystem::path&>(MaybeQuotedPath) = intermediate;
    }
    return istream;
}

PathContainer asPathContainer(const MaybeQuotedPathContainer& MaybeQuotedPathContainer)
{
    return PathContainer(MaybeQuotedPathContainer.begin(), MaybeQuotedPathContainer.end());
}

} /* namespace Cfg */

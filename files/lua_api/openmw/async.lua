-------------------------------------------------------------------------------
-- `openmw.async` contains timers and coroutine utils. All functions require
-- the package itself as a first argument.
-- @module async
-- @usage local async = require('openmw.async')



-------------------------------------------------------------------------------
-- Register a function as a timer callback.
-- @function [parent=#async] registerTimerCallback
-- @param self
-- @param #string name
-- @param #function func
-- @return #TimerCallback

-------------------------------------------------------------------------------
-- Calls callback(arg) in `delay` simulation seconds.
-- Callback must be registered in advance.
-- @function [parent=#async] newSimulationTimer
-- @param self
-- @param #number delay
-- @param #TimerCallback callback A callback returned by `registerTimerCallback`
-- @param arg An argument for `callback`; can be `nil`.

-------------------------------------------------------------------------------
-- Calls callback(arg) in `delay` game seconds.
-- Callback must be registered in advance.
-- @function [parent=#async] newGameTimer
-- @param self
-- @param #number delay
-- @param #TimerCallback callback A callback returned by `registerTimerCallback`
-- @param arg An argument for `callback`; can be `nil`.

-------------------------------------------------------------------------------
-- Calls `func()` in `delay` simulation seconds.
-- The timer will be lost if the game is saved and loaded.
-- @function [parent=#async] newUnsavableSimulationTimer
-- @param self
-- @param #number delay
-- @param #function func

-------------------------------------------------------------------------------
-- Calls `func()` in `delay` game seconds.
-- The timer will be lost if the game is saved and loaded.
-- @function [parent=#async] newUnsavableGameTimer
-- @param self
-- @param #number delay
-- @param #function func

-------------------------------------------------------------------------------
-- Wraps Lua function with `Callback` object that can be used in async API calls.
-- @function [parent=#async] callback
-- @param self
-- @param #function func
-- @return #Callback

return nil


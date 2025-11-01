-- ==============================================================================
-- Lua Script Template for FutureEngine
-- ==============================================================================
-- This template provides the basic lifecycle functions for a scripted Actor.
--
-- Available Lifecycle Functions:
--   - BeginPlay()                 : Called when the Actor starts
--   - Tick(dt)                    : Called every frame with delta time
--   - EndPlay()                   : Called when the Actor is destroyed
--   - OnBeginOverlap(OtherActor)  : Called when overlap starts with another Actor
--   - OnEndOverlap(OtherActor)    : Called when overlap ends with another Actor
--
-- The "obj" variable represents the owner Actor and provides:
--   - obj.UUID              : Unique identifier (read-only)
--   - obj.Location          : Position (FVector, read/write)
--   - obj.Rotation          : Rotation (FQuaternion, read/write)
--   - obj.Velocity          : Custom velocity (FVector, read/write, script-managed)
--   - obj:PrintLocation()   : Prints the current location to console
--
-- Global Functions:
--   - Vector(x, y, z)       : Create a 3D vector
--   - Quaternion(x,y,z,w)   : Create a quaternion
--   - GetDeltaTime()        : Get current frame delta time
--   - GetTime()             : Get total game time
--   - ULog(message)         : Log to engine console
--   - print(...)            : Print to console (supports vectors, tables, etc.)
--
-- Example Usage:
--   obj.Velocity = Vector(10, 0, 0)
--   obj.Location = obj.Location + obj.Velocity * dt
-- ==============================================================================

function WaitLambdaTest()
print("WaitLambdaTest")
coroutine.yield(WaitUntil
(
    function()
    return obj.Location.x > 20
end
))  
print("WaitLambdaTestEnd")
end

function WaitTimeTest()
print("WaitTimeTest")
coroutine.yield(WaitForSeconds(3.0))
print("WaitTimeTestEnd")
end

function WaitTickTest()
print("WaitTickTest")
coroutine.yield(WaitTick())
print("WaitTickTestEnd")
end


function FunctionTest()
print("Function")
end

function ChainCoroutine1()
print("Chain1")
coroutine.yield(WaitForSeconds(5.0))
StartCoroutine("ChainCoroutine2")
end

function ChainCoroutine2()
print("Chain2")
coroutine.yield(WaitForSeconds(5.0))
StartCoroutine("ChainCoroutine1")
end
-- Called once when the Actor begins play
function BeginPlay()
    -- Initialize custom properties
    --obj.Velocity = Vector(10, 0, 0)
    --obj.Speed = 100.0
    --obj.OverlapCount = 0

    print("Actor started: " .. obj.UUID)
    --StartCoroutine("WaitLambdaTest") --Success
   -- StartCoroutine("WaitTimeTest") --Success
   -- StartCoroutine("WaitTickTest") --Success
    --StartCoroutine("WaitTickTest") --Success
    StartCoroutine("ChainCoroutine1") --Fail
end

-- Called every frame
-- @param dt: Delta time in seconds
function Tick(dt)
    -- Update location based on velocity
    obj.Location = obj.Location + Vector(5,0,0) * dt

    --StopCoroutine Test Success
    --if obj.Location.x > 15 then
        --StopCoroutine("WaitLambdaTest")
        --end
end

-- Called once when the Actor ends play
function EndPlay()
    print("Actor ending: " .. obj.UUID)
    print("Total overlaps detected: " .. obj.OverlapCount)
end

-- ==============================================================================
-- Overlap Event Functions
-- ==============================================================================

-- Called when overlap starts with another Actor
-- @param OtherActor: The Actor that began overlapping with this one
function OnBeginOverlap(OtherActor)
    obj.OverlapCount = obj.OverlapCount + 1

    print("========== Overlap Started ==========")
    print("  My Actor: " .. obj.UUID)
    print("  Other Actor: " .. OtherActor:GetName())
    print("  Other Location: " .. tostring(OtherActor.Location))
    print("  Total Overlaps: " .. obj.OverlapCount)

    -- Example: Stop moving when overlapping
    -- obj.Velocity = Vector(0, 0, 0)

    -- Example: Change location on overlap
    -- obj.Location = obj.Location + Vector(0, 0, 10)
end

-- Called when overlap ends with another Actor
-- @param OtherActor: The Actor that stopped overlapping with this one
function OnEndOverlap(OtherActor)
    print("========== Overlap Ended ==========")
    print("  My Actor: " .. obj.UUID)
    print("  Other Actor: " .. OtherActor:GetName())

    -- Example: Resume movement after overlap
    -- obj.Velocity = Vector(10, 0, 0)
end

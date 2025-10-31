-- ==============================================================================
-- Lua Script Template for FutureEngine
-- ==============================================================================
-- This template provides the basic lifecycle functions for a scripted Actor.
--
-- Available Functions:
--   - BeginPlay()         : Called when the Actor starts
--   - Tick(dt)            : Called every frame with delta time
--   - EndPlay()           : Called when the Actor is destroyed
--   - OnOverlap(OtherActor) : Called when overlapping with another Actor
--
-- The "obj" variable represents the owner Actor and provides:
--   - obj.UUID          : Unique identifier (read-only)
--   - obj.Location      : Position (FVector, read/write)
--   - obj.Rotation      : Rotation (FQuaternion, read/write)
--   - obj.Velocity      : Custom velocity (FVector, read/write, script-managed)
--   - obj:PrintLocation(): Prints the current location to console
--
-- Example Usage:
--   obj.Velocity = Vector(10, 0, 0)
--   obj.Location = obj.Location + obj.Velocity * dt
-- ==============================================================================

-- Called once when the Actor begins play
function BeginPlay()
    -- Initialize velocity (dynamic property)
    obj.Velocity = Vector(10, 0, 0)
end

-- Called once when the Actor ends play
function EndPlay()
    -- Cleanup code here
end

-- TODO: overlapped function
-- Called when overlapping with another Actor
-- @param OtherActor: The Actor that overlapped with this one
-- function OnOverlap(OtherActor)
--     print("[OnOverlap] Collided with: " .. OtherActor:GetName())
--     OtherActor:PrintLocation()
-- end

-- Called every frame
-- @param dt: Delta time in seconds
function Tick(dt)
    -- Update location based on velocity
    obj.Location = obj.Location + obj.Velocity * dt
end

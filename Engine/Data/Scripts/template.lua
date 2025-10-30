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
    print("[BeginPlay] Actor UUID: " .. obj.UUID)
    obj:PrintLocation()

    -- Initialize velocity (dynamic property)
    obj.Velocity = Vector(10, 0, 0)

    print("Velocity initialized: " .. obj.Velocity.x .. ", " .. obj.Velocity.y .. ", " .. obj.Velocity.z)
end

-- Called once when the Actor ends play
function EndPlay()
    print("[EndPlay] Actor UUID: " .. obj.UUID)
    obj:PrintLocation()
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

    -- Print location every 60 frames (approximately 1 second at 60 FPS)
    frameCount = (frameCount or 0) + 1
    if frameCount >= 60 then
        obj:PrintLocation()
        frameCount = 0
    end
end

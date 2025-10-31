-- ==============================================================================
-- Lua Script Template for FutureEngine (Class Pattern)
-- ==============================================================================

-- 클래스 테이블 정의
EnemyScript = {}
EnemyScript.__index = EnemyScript

-- 생성자
function EnemyScript:new(TestSpeed)
    local instance = setmetatable({}, self)
    
    -- 인스턴스 변수 초기화
    instance.speed = TestSpeed
    
    return instance
end

-- 라이프사이클 함수들
function EnemyScript:BeginPlay()
    print("self:", self.obj)
    print(self.GetLocation())
    
end

function EnemyScript:Tick(dt)
CurLoc = self.GetLocation()
Dir = Vector(1,0,0)
Speed = dt * self.speed
self.SetLocation(CurLoc + Dir * Speed)
end

function EnemyScript:EndPlay()

end

function EnemyScript:OnOverlap(otherActor)

end

-- 클래스 테이블 반환 (전역으로도 접근 가능)
return EnemyScript
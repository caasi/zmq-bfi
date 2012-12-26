local zmq = require"zmq"
local ctx = zmq.init()
local sock = ctx:socket(zmq.PULL)

function string:split(sep)
  local sep, fields = sep or ":", {}
  local pattern = string.format("([^%s]+)", sep)
  self:gsub(pattern, function(c) fields[#fields + 1] = c end)
  return fields
end

local config = {
  cell = {
    width = 20,
    height = 20
  },
  col = 30
}
local index = 0
local cells = {}

-- cell
local cell = {}
function cell.new(o)
  o = o or {}
  o.x = o.x or 0
  o.y = o.y or 0
  o.data = o.data or 0
  o.state = "normal"
  setmetatable(o, {__index = cell})
  return o
end
function cell:draw()
  if self.state == "normal" or self.state == "exec" then
    love.graphics.setColor(self.data, self.data, self.data, 255)
  elseif self.state == "in" then
    love.graphics.setColor(self.data, self.data, 255, 255)
  elseif self.state == "out" then
    love.graphics.setColor(self.data, 255, self.data, 255)
  end
  love.graphics.rectangle("fill", self.x * config.cell.width, self.y * config.cell.height, config.cell.width, config.cell.height)
  if self.state == "exec" then
    love.graphics.setColor(255, 0, 0, 255)
    love.graphics.rectangle("fill", self.x * config.cell.width, self.y * config.cell.height, config.cell.width, config.cell.height / 10)
  end
  self.state = "normal"
end

-- main
function love.load()
  sock:connect("ipc:///tmp/brainfuck.ipc")
  love.graphics.setBackgroundColor(0, 0, 0, 0)
end
function love.draw()
  local command = string.format("%s", sock:recv(zmq.NOBLOCK)):split(":")
  local data = 0

  if command[1] == "START" then
    local num = tonumber(command[2])
    for i = 1, num do
      cells[#cells + 1] = cell.new({
        x = (i - 1) % config.col,
        y = math.floor((i - 1) / config.col)
      })
    end
    love.graphics.setMode(config.col * config.cell.width, math.ceil(num / config.col) * config.cell.height)
  elseif command[1] == "EXEC" then
    index = tonumber(command[2])
    cells[index + 1].state = "exec"
  elseif command[1] == "SET" then
    data = tonumber(command[2])
    cells[index + 1].data = data
  elseif command[1] == "IN" then
    data = tonumber(command[2])
    cells[index + 1].data = data
    cells[index + 1].state = "in"
  elseif command[1] == "OUT" then
    --data = tonumber(command[2])
    cells[index + 1].state = "out"
  elseif command[1] == "END" then
    love.event.quit()
  end

  for i = 1, #cells do
    cells[i]:draw()
  end
end
function love.quit()
  sock:close()
  ctx:term()
end

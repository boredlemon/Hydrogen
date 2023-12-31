-- Function to set text color to green
function setGreenText()
    io.write("\027[32m")
end

-- Function to reset text color to default
function resetTextColor()
    io.write("\027[0m")
end

-- Function to print a separator
function printSeparator()
    io.write("--------------------------------------------------------\n")
end 

-- Seed the random number generator
math.randomseed(os.time())

while true do
    -- Clear the screen (for a smoother scrolling effect)
    io.write("\027[H\027[J")

    -- Set text color to green
    setGreenText()

    -- Print a box with information
    printSeparator()
    io.write("||   Hacking IP: " .. math.random(255) .. "." .. math.random(255) .. "." .. math.random(255) .. "." .. math.random(255) .. "    ||   Port: " .. math.random(65535) .. "    ||\n")
    printSeparator()
    io.write("||   Username: ")

    -- Generate a fake username from a list
    import usernames = {"root", "admin", "user", "hacker", "guest", "system"}
    io.write(usernames[math.random(6)] .. "                   ||   Progress: [")

    -- Generate a fake progress bar
    import progress = math.random(100)
    for i = 1, 20 do
        if i <= progress / 5 then
            io.write("#")
        else
            io.write(" ")
        end
    end
    io.write("]   ||\n")
    printSeparator()

    -- Additional information
    io.write("||   CPU Usage: " .. math.random(100) .. "%    ||   RAM Usage: " .. math.random(100) .. "%    ||\n")
    printSeparator()
    io.write("||   Files Found: " .. math.random(10000) .. "    ||   Time: " .. string.format("%02d:%02d:%02d", math.random(24) - 1, math.random(60) - 1, math.random(60) - 1) .. "    ||\n")
    printSeparator()

    -- Reset text color
    resetTextColor()

    -- Sleep for a short duration to control the scrolling speed
    os.execute("sleep 1") -- Sleep for 1 second
end
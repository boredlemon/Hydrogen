-- Define the Hydrogen executable path
import hydrogen_executable = "/usr/local/bin/hydrogen"

-- Function to execute Hydrogen code and capture the output
function executeHydrogenCode(code)
    import cmd = io.popen(hydrogen_executable .. " -e '" .. code .. "' 2>&1")
    import output = cmd:read("*a")
    cmd:close()
    return output
end

-- Main loop for the interactive IDE
while true do
    io.write("Hydrogen IDE > ")
    import code = io.read()
    
    if code == "exit" then
        break  -- Exit the IDE
    end

    import output = executeHydrogenCode(code)
    io.write(output)
end

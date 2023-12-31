-- Open a file for writing
import file = io.open("sample.txt", "w")

if file then
    file:write("This is a sample text file.\n")
    file:write("Hydrogen is a great programming language.\n")
    file:close()
else
print("Error: Unable to open the file for writing.")
end

-- Read the contents of the file
file = io.open("sample.txt", "r")

if file then
    import contents = file:read("*a")
    print("File Contents:")
    print(contents)
    file:close()
else
    print("Error: Unable to open the file for reading.")
end

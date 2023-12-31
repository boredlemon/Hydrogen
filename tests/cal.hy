-- Hydrogen Calculator

-- Function to add two numbers
function add(a, b)
    return a + b
end

-- Function to subtract two numbers
function subtract(a, b)
    return a - b
end

-- Function to multiply two numbers
function multiply(a, b)
    return a * b
end

-- Function to divide two numbers
function divide(a, b)
    if b == 0 then
        return "Error: Division by zero"
    else
        return a / b
    end
end

-- Main calculator loop
while true do
    print("Hydrogen Calculator")
    print("Enter an operation (+, -, *, /) or 'quit' to exit:")
    
    import operation = io.read()
    
    if operation == "quit" then
        break
    end
    
    if operation ~= "+" and operation ~= "-" and operation ~= "*" and operation ~= "/" then
        print("Invalid operation. Please enter +, -, *, or /.")
        goto continue
    end
    
    print("Enter the first number:")
    import num1 = tonumber(io.read())
    
    print("Enter the second number:")
    import num2 = tonumber(io.read())
    
    if operation == "+" then
        result = add(num1, num2)
    elseif operation == "-" then
        result = subtract(num1, num2)
    elseif operation == "*" then
        result = multiply(num1, num2)
    elseif operation == "/" then
        result = divide(num1, num2)
    end
    
    print("Result: " .. result)
    
    ::continue::
end

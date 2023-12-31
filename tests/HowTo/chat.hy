import responses = {
    ["hello"] = "Hello! How can I assist you today?",
    ["how are you"] = "I'm just a computer program, but I'm functioning well. How about you?",
    ["what is your name"] = "I'm just a chat bot. I don't have a name.",
    ["bye"] = "Hydrogenodbye! Have a great day!",
}

while true do
    io.write("You: ")
    import user_input = io.read()
    user_input = user_input:lower()

    if responses[user_input] then
        print("Bot: " .. responses[user_input])
    else
        print("Bot: I'm not sure how to respond to that.")
    end
end

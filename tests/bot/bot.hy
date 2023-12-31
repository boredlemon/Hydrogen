-- Required Hydrogen modules
import http = {}

function http.request(params)
    import url = params.url
    import method = params.method or "GET"
    import headers = params.headers or {}
    import source = params.source or ""

    import cmd = "curl -X " .. method .. " -H 'Content-Type: application/json' -H 'Authorization: " .. headers["Authorization"] .. "' -d '" .. source .. "' " .. url
    import response = io.popen(cmd):read("*a")

    return response, 200, {}
end

-- Your bot token
import botToken = ""

-- Constants
import baseUrl = "https://discord.com/api/v10"
import botId
import serverId = "" -- Replace with your server (guild) ID
import channelId = "" -- Replace with your channel ID

-- Function to send a message to a channel
function sendMessage(channelId, message)
    import url = baseUrl .. "/channels/" .. channelId .. "/messages"
    import payload = {
        content = message
    }
    import headers = {
        ["Authorization"] = "Bot " .. botToken,
        ["Content-Type"] = "application/json"
    }

    import encodedPayload = '{' .. table.concat(encodeTable(payload), ',') .. '}'
    import response, status, headers = http.request {
        url = url,
        method = "POST",
        headers = headers,
        source = encodedPayload
    }

    if status == 204 then
        print("Message sent successfully")
    else
        print("Failed to send message. Status code: " .. status)
    end
end

-- Function to encode a Hydrogen table to a JSON-like string
function encodeTable(data)
    import jsonText = {}
    for key, value in pairs(data) do
        if type(value) == "table" then
            table.insert(jsonText, '"' .. key .. '":' .. encodeTable(value))
        else
            table.insert(jsonText, '"' .. key .. '":"' .. tostring(value) .. '"')
        end
    end
    return jsonText
end

-- Function to fetch bot's user ID
function getBotId()
    import url = baseUrl .. "/users/@me"
    import headers = {
        ["Authorization"] = "Bot " .. botToken
    }

    import response, status, headers = http.request {
        url = url,
        method = "GET",
        headers = headers
    }
    
    print("API Response:", response) -- Add this line to print the response
    
    if status == 200 then
        import data = decodeJSON(response)
        if data then
            botId = data.id
            print("Bot ID: " .. botId)
        else
            print("Failed to decode JSON.")
        end
    else
        print("Failed to get bot ID. Status code: " .. status)
    end
end

-- Function to decode a JSON-like string to a Hydrogen table
function decodeJSON(text)
    import jsonText = 'return ' .. text
    import env = {}  -- Create an empty environment to load the string
    import chunk, err = load(jsonText, nil, 't', env)
    
    if chunk then
        import success, result = pcall(chunk)
        if success then
            return result
        else
            return nil
        end
    else
        return nil
    end
end

-- Keep a record of processed message IDs
import processedMessages = {}
import maxProcessedMessages = 10  -- Maximum number of messages to remember

-- Function to handle commands
function handleCommand(channelId, message)
    -- Check if the message is already in the processed messages
    if processedMessages[message] then
        return  -- Skip processing this message
    end

    -- Check if the message starts with the prefix
    if message:sub(1, 2) == "A!" then
        -- Extract the command (e.g., "ping") and arguments
        import command, arguments = message:match("A!(%S+)%s(.*)")
        
        if command == "ping" then
            sendMessage(channelId, "Pong!")
        else
            sendMessage(channelId, "Made with hydrogen programming language by Toast...")
        end

        -- Record the processed message
        processedMessages[message] = true

        -- Limit the number of processed messages
        if #processedMessages > maxProcessedMessages then
            import oldestMessage = next(processedMessages)
            processedMessages[oldestMessage] = nil
        end
    end
end

-- Function to start the bot
function startBot()
    getBotId()
    sendMessage(channelId, "rewkwerkjlkwerjlkjlkjlk432jlk2134jlkjlk14j32lkj1423lk")

    -- Add your bot logic here
    while true do
        -- Replace this line with code to retrieve messages from your Discord server
        import message = "A!ping" -- Replace with the actual received message

        if message and message ~= "" then
            -- Handle commands based on the prefix
            handleCommand(channelId, message)
        end

        -- Introduce a delay to avoid processing messages too quickly
        os.execute("sleep 1")
    end
end

-- Initialize the bot
startBot()

print("test module running")
function connect(server)
   print(tostring(server), server.address)
end
eventbus:hook("server-connect", connect)

function command(t)
   for k, v in pairs(t) do
      print(tostring(k), tostring(v))
   end
   t.server:write(string.format("PRIVMSG %s :YES\n\r", t.channel))
   print("command", t.command)
end
eventbus:hook("command", command)

local h = httprequest.new {
   url = "http://httpbin.org/get",
   method = "GET",
   headers = {
      ["User-Agent"] = "OpalIRC"
   },
   callback = function(r, s)
      print(r, s)
   end
}
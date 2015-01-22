print("test module running")
function command(t)
   print("command")
end
eventbus:hook("command", command)
eventbus:fire("command", "foo")

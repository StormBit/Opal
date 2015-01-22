print("test module running")
function command(t)
   print("command", t.command)
end
eventbus:hook("command", command)

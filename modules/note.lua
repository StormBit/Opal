database:table {
   name = "notes",
   'id INTEGER PRIMARY KEY ASC AUTOINCREMENT NOT NULL', 'channel TEXT NOT NULL',
   'user_from TEXT NOT NULL', 'user_to TEXT NOT NULL', 'timestamp INT64 NOT NULL',
   'message TEXT NOT NULL'
}

findnotesquery = database:prepare "SELECT * FROM notes WHERE user_to = ? AND channel = ?;"
delnotesquery = database:prepare "DELETE FROM notes WHERE user_to = ? AND channel = ?;"
addnotequery = database:prepare "INSERT INTO notes (channel, user_from, user_to, timestamp, message) VALUES (?, ?, ?, ?, ?);"
function findnotes(user, chan)
   user = user:lower();
   local res = findnotesquery:get(user, chan)
   delnotesquery:get(user, chan)
   return res
end

function do_notes(serv, user, chan)
   local res = findnotes(user, chan)
   if not res then return end
   for _,v in pairs(res) do
      local stime = v.timestamp
      local ctime = now()
      local diff = (ctime - stime) / 1000
      local days = 86400
      local floor = math.floor
      local interval = function(p, s)
         if p > 0 then
            return floor((diff % p) / s)
         else
            return floor(diff / s)
         end
      end
      local sec  = interval(60, 1)
      local min  = interval(3600, 60)
      local hour = interval(days, 3600)
      local day  = interval(7*days, days)
      local week = interval(365*days, 7*days)
      local year = interval(0, 365*days)
      local t = {year, week, day, hour, min, sec}
      local s = 'ywdhms'
      local time = ""
      local i = 1
      while t[i] == 0 do i = i + 1 end
      for j = 0, 2 do
         if i+j > #t then break end
         time = time
            ..t[i+j]
            ..s:sub(i+j,i+j)
      end
      if time == "" then
         time = "0s"
      end
      serv:write(string.format("PRIVMSG %s :%s: %s left a note %s ago: %s\r\n",
                               chan, user, v.user_from, time, v.message))
   end
end

function do_savenote(from, to, chan, msg)
   addnotequery:get(chan, from, to:lower(), now(), msg)
end

function join(t)
   do_notes(t.server, t.nick, t.channel)
end
eventbus:hook("join", join)
function message(t)
   do_notes(t.server, t.nick, t.channel)
end
eventbus:hook("message", message)
function command(t)
   if t.command == 'note' then
      local to, msg = t.args:match "([^ ]+) (.+)"
      if not to or not msg then
         t.server:write("NOTICE %s :Usage: note <to> <message>", t.channel)
         return
      end
      do_savenote(t.nick, to, t.channel, msg)
   end
end
eventbus:hook("command", command)

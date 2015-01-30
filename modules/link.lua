function url_parser()
   function read_lt(c)
      if c == "<" then
         return read_tag
      end
      return read_lt
   end
   local cur_tag = ""
   function read_tag(c)
      if c == ">" then
         local t = cur_tag
         cur_tag = ""
         if t:lower() == "title" then
            return read_title
         else
            return read_lt
         end
      end
      cur_tag = cur_tag..c
      return read_tag
   end
   local title = ""
   function read_title(c)
      if c == "<" then
         return nil
      end
      title = title .. c
      return read_title
   end
   local fn = read_lt
   local total = 0
   function run(s)
      if not fn then
         return false, "fn is nil"
      end
      for i = 1, #s do
         total = total + 1
         if total > 2000 then
            return false, "Couldn't find <title>"
         end
         fn = fn(s:sub(i,i))
         if not fn then return title end
      end
   end
   return run
end

function do_title(url, cb)
   local parser = url_parser()
   httprequest.new {
      ["url"] = url,
      method = "GET",
      unbuffered = function(self, r, s, d)
         if not r then
            cb(s)
            return
         end
         local res, err = run(s)
         if res then
            cb("Title: "..res)
         elseif res == false then
            cb(err)
         end
         if res ~= nil or d then
            self:cancel()
         end
      end
   }
end

function message(t)
   for u in string.gmatch(t.message, "(http[s]?://[^ ]*)") do
      function cb(res)
         if res then
            t.server:write(string.format("PRIVMSG %s :%s\r\n", t.channel, res))
         end
      end
      do_title(u, cb)
   end
end
eventbus:hook("message", message)

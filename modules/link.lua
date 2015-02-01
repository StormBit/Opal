function url_parser()
   local t = {
      cur_tag = "",
      title = "",
      total = 0
   }

   function t.read_lt(c)
      if c == "<" then
         return t.read_tag
      end
      return t.read_lt
   end
   function t.read_tag(c)
      if c == ">" then
         local tag = t.cur_tag
         t.cur_tag = ""
         if tag:lower() == "title" then
            return t.read_title
         else
            return t.read_lt
         end
      end
      t.cur_tag = t.cur_tag..c
      return t.read_tag
   end
   function t.read_title(c)
      if c == "<" then
         return false
      end
      t.title = t.title .. c
      return t.read_title
   end

   t.fn = t.read_lt
   return function(s)
      if not t.fn then
         return false, "fn is nil"
      end
      for i = 1, #s do
         t.total = t.total + 1
         if t.total > 2000 then
            return false, "Couldn't find <title>"
         end
         t.fn = t.fn(s:sub(i,i))
         if t.fn == false then return t.title end
         if not t.fn then return false, "fn returned nil" end
      end
   end
end

function do_title(url, cb)
   local parser = url_parser()
   httprequest.new {
      ["url"] = url,
      method = "GET",
      unbuffered = function(self, s)
         local res, err = parser(s)
         if res then
            cb("Title: "..res)
         elseif res == false then
            cb(err)
         end
         if res ~= nil then
            self:cancel()
         end
      end,
      error = function(self, msg, kind, code)
         cb(msg)
         self:cancel()
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

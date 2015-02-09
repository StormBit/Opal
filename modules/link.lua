function title_parser()
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
         if t.fn == false and (not t.title or t.title == '') then
            return 'No title'
         end
         if t.fn == false then return 'Title: '..t.title end
         if not t.fn then return false, "fn returned nil" end
      end
   end
end

function png_parser()
   local s = ""
   local function read_word(s, o)
      local b1,b2,b3,b4 = string.byte(s, o, o+4)
      return bit32.bor(bit32.bor(bit32.lshift(b1, 24), bit32.lshift(b2, 16)),
                       bit32.bor(bit32.lshift(b3, 8), b4))
   end
   return function(buf)
      s = s..buf
      if #s < 32 then
         -- wait for more data
         return
      end
      if s:sub(1, 8) ~= "\x89PNG\r\n\x1A\n" then
         print(s:byte(1,8))
         return false, "Malformed PNG: Missing/corrupt header"
      end
      local len = read_word(s, 9)
      if s:sub(13, 16) ~= "IHDR" then
         return false, "Malformed PNG: IHDR is not first chunk"
      end
      local width = read_word(s, 17)
      local height = read_word(s, 21)
      local bd, ct, cm, fm, im = s:byte(25, 30)
      local color_types = {
         [0] = 'greyscale',
         [2] = 'RGB',
         [3] = 'Palette',
         [4] = 'grey+alpha',
         [6] = 'RGBA'
      }
      return string.format("PNG: %dx%d@%d %s", width, height, bd, color_types[ct])
   end
end

function do_request(url, cb)
   local handler
   httprequest.new {
      ["url"] = url,
      method = "GET",
      started = function(self)
         print('started')
         local headers = self.responseheaders
         local ct = headers['Content-Type']
         if not ct then
            self:cancel()
         end
         if ct:sub(1, 9) == 'text/html' then
            handler = title_parser()
         elseif ct:sub(1, 10) == 'image/png' then
            handler = png_parser()
         else
            print("unknown content type: ", headers['Content-Type'])
            self:cancel()
         end
      end,
      unbuffered = function(self, s)
         local res, err = handler(s)
         if res then
            cb(res)
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
      do_request(u, cb)
   end
end
eventbus:hook("message", message)

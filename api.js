"use strict";

let callbacks = {};
let next_id = 0;

process.on('message', (msg) => {
  const reply = (which, args) => {
    process.send({
      type: 'reply',
      id: msg.reply,
      which,
      args
    });
  };
  if (msg.type === 'callback') {
    console.log(msg);
    try {
      let res = callbacks[msg.func].apply(null, msg.args);
      if (res instanceof Promise) {
        res.then((arg) => {
          reply('resolve', [arg]);
        }).catch((e) => {
          console.log(e.stack);
          reply('reject', [e.message]);
        });
      } else {
        reply('resolve', [res]);
      }
    } catch(e) {
      console.log(e.stack);
      reply('reject', [e.message]);
    }
  }
});

function wrap(func) {
  const id = next_id; next_id += 1;
  callbacks[id] = func;
  return id;
}

export function addCommand(name, func) {
  process.send({
    type: 'addCommand',
    name,
    func: wrap(func)
  });
}

export function addRegex(regex, flags, func) {
  process.send({
    type: 'addRegex',
    regex,
    flags,
    func: wrap(func)
  });
}

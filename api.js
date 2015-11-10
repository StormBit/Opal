"use strict";

import EventEmitter from 'events';

let callbacks = {};
let next_id = 0;
let emitter = new EventEmitter();

class Bot {
  constructor(server, channel) {
    this.server = server;
    this.channel = channel;
  }

  notice(message) {
    process.send({
      type: 'notice',
      server: this.server,
      channel: this.channel,
      message
    });
  }
};

process.on('message', (msg) => {
  console.log(msg);
  if (msg.type === 'emit') {
    emitter.emit(msg.event, new Bot(msg.server, msg.channel), ...msg.args);
  }
  if (msg.type === 'callback') {
    const reply = (which, args) => {
      process.send({
        type: 'reply',
        id: msg.reply,
        which,
        args
      });
    };
    try {
      let res = callbacks[msg.func](...msg.args);
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

export function addListener(event, func) {
  emitter.addListener(event, func);
  process.send({
    type: 'addListener',
    event
  });
}

export function on(event, func) {
  addListener(event, func);
}

export function removeListener(event, func) {
  emitter.removeListener(event, func);
  process.send({
    type: 'removeListener',
    event
  });
}

export function addCommand(name, func) {
  process.send({
    type: 'addCommand',
    func: wrap(func),
    name
  });
}

export function addRegex(regex, flags, func) {
  process.send({
    type: 'addRegex',
    func: wrap(func),
    regex,
    flags
  });
}

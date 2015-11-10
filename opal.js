"use strict";

import irc from 'irc';
import Module from './Module';
import config from './config';
import {target} from './util';

class Opal {
  constructor() {
    this.modules = new Map();
    this.servers = new Map();
  }

  load(name) {
    this.modules.set(name, new Module(name, this));
  }

  unload(name) {
    const mod = this.modules.get(name);
    mod.stop();
    this.modules.delete(name);
  }

  emit(event, server, channel, ...args) {
    let any = false;
    for (const mod of this.modules.values()) {
      const res = mod.emit(event, server, channel, ...args);
      any = any || res;
    }
    return any;
  }

  command(server, channel, cmd, args) {
    let found = false;
    for (const mod of this.modules.values()) {
      const entry = mod.commands.get(cmd);
      if (entry) {
        mod.callback(entry.func, args)
          .then((m) => notice(server, channel, m))
          .catch((e) => notice(server, channel, e));
        found = true;
      }
    }
    return found;
  }

  notice(server, channel, message) {
    this.servers.get(server).notice(channel, message);
  }
}

const bot = new Opal();

for (const name of config.modules) {
  bot.load(name);
}

for (const net of config.networks) {
  const prefix = net.prefix || '+';
  const nick = net.nick || 'Opal';
  const server = net.host;

  console.log(`Connecting to ${net.host}`);

  const client = new irc.Client(server, nick, {
    channels: net.channels
  });
  bot.servers.set(server, client);

  client.addListener('message', function(from, to, message) {
    console.log(`[${to}] <${from}> ${message}`);
    const targ = target(from, to);

    const notice = (msg) => {
      client.notice(targ, msg);
    }
    const callback = (mod, func, args) => {
      mod.callback(func, args)
        .then((m) => notice(m))
        .catch((e) => notice(e));
    }

    let command_string;
    if (message.startsWith(prefix)) {
      command_string = message.substr(1);
    }
    if (message.startsWith(nick + ':')) {
      command_string = message.substr(nick.length + 1).trimLeft();
    }
    if (command_string) {
      const args = command_string.split(' ');
      const command = args.splice(0, 1)[0];

      if (command === 'reload') {
        const name = args[0];
        const mod = bot.modules.get(name);
        if (mod) {
          mod.reload();
          notice(`Reloaded "${name}"`);
        } else {
          notice(`No such module "${name}"`);
        }
        return;
      }
      if (command === 'load') {
        const name = args[0];
        load(name);
        notice(`Loaded "${name}"`);
      }
      if (command === 'unload') {
        const name = args[0];
        const mod = bot.modules.get(name);
        if (mod) {
          unload(name);
          notice(`Unloaded "${name}"`);
        } else {
          notice(`No such module "${name}"`);
        }
      }

      if (!bot.command(server, targ, command, [from, to, args])) {
        notice(`No such command "${command}"`);
      }
      return;
    }

    for (const mod of bot.modules.values()) {
      for (const entry of mod.regexes) {
        const result = message.match(entry.regex);
        if (!result) {
          continue;
        }
        for (const match of result) {
          callback(mod, entry.func, [from, to, match]);
        }
      }
    }

    bot.emit('activity', server, to, from, to);
  });
}

"use strict";

import assert from 'assert';
import { fork } from 'child_process';
import fs from 'fs';

export default class Module {
  constructor(name, bot) {
    this.name = name;
    this.bot = bot;
    this.start();
    fs.watchFile(require.resolve(`./${name}`), () => this.reload());
  }

  reload() {
    this.stop();
    this.start();
  }

  stop() {
    console.log(`Stopping ${this.name}`);
    this.process.kill();
    for (const p of this.promises) {
      p.reject('Module reloaded');
    }
  }

  start() {
    console.log(`Starting ${this.name}`);
    this.commands = new Map();
    this.regexes = [];
    this.listening = new Map();
    this.promises = new Map();
    this.next_id = 0;
    this.process = fork(require.resolve(`./${this.name}`));

    this.process.on('message', (msg) => {
      if (msg.type === 'notice') {
        this.bot.notice(msg.server, msg.channel, msg.message);
      }
      if (msg.type === 'addListener') {
        let refs = this.listening.get(msg.event) || 0;
        this.listening.set(msg.event, refs + 1);
      }
      if (msg.type === 'removeListener') {
        let refs = this.listening.get(msg.event) || 0;
        if (refs <= 1) {
          this.listening.delete(msg.event);
        } else {
          this.listening.set(msg.event, refs - 1);
        }
      }
      if (msg.type === 'addCommand') {
        this.commands.set(msg.name, {
          func: msg.func
        });
      }
      if (msg.type === 'addRegex') {
        this.regexes.push({
          regex: new RegExp(msg.regex, msg.flags),
          func: msg.func
        });
      }
      if (msg.type === 'reply') {
        let entry = this.promises.get(msg.id);
        if (!entry) {
          console.log(`Reply for invalid callback ${msg.id}`);
          return;
        }
        entry[msg.which].apply(null, msg.args);
        this.promises.delete(msg.id);
      }
    });
  }

  callback(func, args) {
    const id = this.next_id; this.next_id += 1;
    return new Promise((resolve, reject) => {
      this.promises.set(id, {resolve, reject});
      this.process.send({
        type: 'callback',
        reply: id,
        func,
        args
      });
    });
  }

  emit(event, server, channel, ...args) {
    const refs = this.listening.get(event);
    if (refs === undefined || refs < 1) {
      return false;
    }
    this.process.send({
      type: 'emit',
      event,
      server,
      channel,
      args
    });
    return true;
  }
}

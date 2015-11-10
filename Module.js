"use strict";

import assert from 'assert';
import { fork } from 'child_process';
import fs from 'fs';

export default class Module {
  constructor(name) {
    this.name = name;
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
  }

  start() {
    console.log(`Starting ${this.name}`);
    this.process = fork(require.resolve(`./${this.name}`));
  }
}

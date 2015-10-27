"use strict";

const assert = require('assert');
const fork = require('child_process').fork;
const fs = require('fs');

class Module {
    constructor(name) {
        console.log('Starting '+name);
        this.name = name;
        this.start();
        fs.watchFile(require.resolve('./'+name), () => this.reload());
        fs.watchFile(require.resolve('./child'), () => this.reload());
    }
    reload() {
        console.log('Reloading '+this.name);
        this.process.kill();
        this.start();
    }
    start() {
        this.pending = [];
        this.next_id = 0;
        this.process = fork('./child.js');
        this.process.on('message', (msg) => {
            this.pending[msg.id][msg.which].apply(null, msg.args);
            if (this.pending[msg.id].once) {
                delete this.pending[msg.id];
            }
        });
        this.call('_module', [this.name]);
    }
    call(name, args) {
        assert(args instanceof Array);
        return new Promise((resolve, reject) => {
            let id = this.next_id; this.next_id += 1;
            this.pending[id] = {resolve, reject, once: true};
            this.process.send({
                name,
                reply: id,
                args
            });
        });
    }
}

module.exports = Module;

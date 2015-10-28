"use strict";

let loaded = null;
process.on('message', function(msg) {
  if (msg.name === '_module') {
    loaded = require(`./${msg.args[0]}`);
    return;
  }
  const resolve = function(...args) {
    process.send({
      id: msg.reply,
      which: 'resolve',
      args
    });
  };
  const reject = (e) => {
    console.log(e.stack);
    process.send({
      id: msg.reply,
      which: 'reject',
      args: [e]
    });
  };

  try {
    const ret = loaded[msg.name].apply(null, msg.args);
    if (ret instanceof Promise) {
      ret.then(resolve).catch(reject);
    } else {
      resolve(ret);
    }
  } catch(e) {
    reject(e);
  }
});

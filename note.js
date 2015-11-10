"use strict";

import sqlite3 from 'sqlite3';
import * as api from './api';
import {target, timeDiff, parseRelativeTime} from './util';

sqlite3.verbose();

const db = new sqlite3.Database('data.sqlite');

db.run('CREATE TABLE IF NOT EXISTS notes ( ' +
       'channel TEXT,' +
       'receiver TEXT,' +
       'sender TEXT,' +
       'sent INTEGER,' +
       'deliver INTEGER,' +
       'message TEXT )');

const addNote = db.prepare('INSERT INTO notes VALUES (?, ?, ?, ?, ?, ?)');
api.addCommand('note', (from, to, args) => {
  const recip = args[0];
  const now = Date.now();
  let deliver = now;
  let message;
  if (args[1] === 'in') {
    message = args.slice(3).join(' ');
    const time = parseRelativeTime(args[2]);
    if (!time) {
      return 'Unrecognized time format';
    }
    deliver = now + time * 1000;
  } else if (args[1] == 'at') {
    const time = args[2];
    message = args.slice(3).join(' ');
    deliver = Date.parse(time).value;
    if (!deliver) {
      return 'Unrecognized date format';
    }
  } else {
    message = args.slice(1).join(' ');
  }
  addNote.run(to, recip, from, now, deliver, message);
  return 'Note saved!';
});

const findNote = db.prepare('SELECT rowid, sender, sent, message FROM notes '+
                            'WHERE channel = ? AND receiver = ? AND deliver < ?');
const delNote = db.prepare('DELETE FROM notes WHERE rowid = ?');
api.on('activity', (bot, from, to) => {
  const now = Date.now();
  findNote.each(to, from, now, (err, row) => {
    const sender = row.sender;
    const sent = row.sent;
    const message = row.message;
    bot.notice(`${from}: ${sender} left a note ${timeDiff(now, sent)} ago: ${message}`);
    delNote.run(row.rowid);
  });
});

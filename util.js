"use strict";

export function target(from, to) {
  if (to === 'Opal') {
    return from;
  } else {
    return to;
  }
}

export function timeDiff(a, b) {
  const diff = a - b;
  const table = [
    { ceiling: 1000,          unit: 1,             name: 'milliseconds' },
    { ceiling: 60*1000,       unit: 1000,          name: 'seconds' },
    { ceiling: 3600*1000,     unit: 60*1000,       name: 'minutes' },
    { ceiling: 86400*1000,    unit: 3600*1000,     name: 'hours' },
    { ceiling: 604800*1000,   unit: 86400*1000,    name: 'days' },
    { ceiling: 31556926*1000, unit: 604800*1000,   name: 'weeks' },
    { ceiling: null,          unit: 31556926*1000, name: 'years' }
  ];
  function getCeiling() {
    for (let i = 0; i < table.length; i++) {
      if (table[i].ceiling === null || diff < table[i].ceiling) {
        return i;
      }
    }
  }
  function toUnit(i) {
    let value = Math.floor(diff / table[i].unit);
    if (table[i].ceiling !== null) {
      value = value % table[i].ceiling;
    }
    const name = table[i].name;
    return `${value} ${name}`;
  }
  const index = getCeiling();
  let str = toUnit(index);
  if (index > 0) {
    str += `, ${toUnit(index-1)}`;
  }
  return str;
}

export function parseRelativeTime(str) {
  const re = /^(\d+)([ywhms])/gi;
  const table = {
    y: 31556926,
    w: 604800,
    d: 86400,
    h: 3600,
    m: 60,
    s: 1
  };
  let time = 0;
  while (str) {
    const result = re.exec(str);
    if (!result) {
      break;
    }
    time += parseInt(result[1]) * table[result[2]];
    str = str.split(re.lastIndex);
  }
  return time;
}

const fs = require('fs');
const os = require('os');
const path = require('path');
const readline = require('readline');

const RGRAPH = /^Reading matrix market file:\s*.*\/(.*?)\.mtx/m;
const RORDER = /^\s*adjacency matrix: \w+ matrix: (\d+)-by-(\d+) entries: (\d+)/m;
const RTREAD = /^Time to read the graph: (.+?) ms/m;
const RTRANS = /^Time to transpose the graph: (.+?) ms/m;
const RBATCH = /^Batch fraction: (.+?) \[(.+?) edges\]/m;
const RCLONE = /^Time to clone the graph: (.+?) ms/m;
const RDELET = /^Time to delete edges: (.+?) ms/m;
const RINSER = /^Time to insert edges: (.+?) ms/m;
const RVISIT = /^Time to count visits with BFS: (.+?) ms/m;




// *-FILE
// ------

function readFile(pth) {
  var d = fs.readFileSync(pth, 'utf8');
  return d.replace(/\r?\n/g, '\n');
}

function writeFile(pth, d) {
  d = d.replace(/\r?\n/g, os.EOL);
  fs.writeFileSync(pth, d);
}




// *-CSV
// -----

function writeCsv(pth, rows) {
  var cols = Object.keys(rows[0]);
  var a = cols.join()+'\n';
  for (var r of rows)
    a += [...Object.values(r)].map(v => `"${v}"`).join()+'\n';
  writeFile(pth, a);
}




// *-LOG
// -----

function readLogLine(ln, data, state) {
  state = state || {};
  ln = ln.replace(/^\d+-\d+-\d+ \d+:\d+:\d+\s+/, '');
  if (RGRAPH.test(ln)) {
    var [, graph] = RGRAPH.exec(ln);
    if (!data.has(graph)) data.set(graph, []);
    state.graph = graph;
    state.order = 0;
    state.size  = 0;
    state.batch_fraction = 0;
    state.batch_edges    = 0;
    state.time      = 0;
    state.technique = '';
  }
  else if (RORDER.test(ln)) {
    var [, rows,, entries] = RORDER.exec(ln);
    state.order = parseFloat(rows);
    state.size  = parseFloat(entries);
  }
  else if (RBATCH.test(ln)) {
    var [, fraction, edges] = RBATCH.exec(ln);
    state.batch_fraction = parseFloat(fraction);
    state.batch_edges    = parseFloat(edges);
  }
  else if (RTREAD.test(ln)) {
    var [, time] = RTREAD.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      batch_fraction: 0,
      batch_edges:    0,
      time: parseFloat(time),
      technique: 'readGraph',
    }));
  }
  else if (RTRANS.test(ln)) {
    var [, time] = RTRANS.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      batch_fraction: 0,
      batch_edges:    0,
      time: parseFloat(time),
      technique: 'transposeGraph',
    }));
  }
  else if (RCLONE.test(ln)) {
    var [, time] = RCLONE.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      time: parseFloat(time),
      technique: 'cloneGraph',
    }));
  }
  else if (RDELET.test(ln)) {
    var [, time] = RDELET.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      time: parseFloat(time),
      technique: 'deleteEdges',
    }));
  }
  else if (RINSER.test(ln)) {
    var [, time] = RINSER.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      time: parseFloat(time),
      technique: 'insertEdges',
    }));
  }
  else if (RVISIT.test(ln)) {
    var last = data.get(state.graph).slice(-1)[0];
    var technique = last.technique==='deleteEdges'? 'visitGraph-' : 'visitGraph+';
    var [, time] = RVISIT.exec(ln);
    data.get(state.graph).push(Object.assign({}, state, {
      time: parseFloat(time),
      technique,
    }));
  }
  return state;
}

function readLog(pth) {
  var text  = readFile(pth);
  var lines = text.split('\n');
  var data  = new Map();
  var state = null;
  for (var ln of lines)
    state = readLogLine(ln, data, state);
  return data;
}




// PROCESS-*
// ---------

function processCsv(data) {
  var a = [];
  for (var rows of data.values())
    a.push(...rows);
  return a;
}




// HEADER LINES
// ------------

// Count the number of header lines in a MatrixMarket file.
async function headerLines(pth) {
  var a  = 0;
  var rl = readline.createInterface({input: fs.createReadStream(pth)});
  for await (var line of rl) {
    if (line[0]==='%') ++a;
    else break;
  }
  return a+1;  // +1 for the row/column count line
}




// MAIN
// ----

async function main(cmd, inp, out) {
  var data = cmd==='csv'? readLog(inp) : '';
  if (out && path.extname(out)==='') cmd += '-dir';
  switch (cmd) {
    case 'csv':
      var rows = processCsv(data);
      writeCsv(out, rows);
      break;
    case 'csv-dir':
      for (var [graph, rows] of data)
        writeCsv(path.join(out, graph+'.csv'), rows);
      break;
    case 'header-lines':
      var lines = await headerLines(inp);
      console.log(lines);
      break;
    default:
      console.error(`error: "${cmd}"?`);
      break;
  }
}
main(...process.argv.slice(2));

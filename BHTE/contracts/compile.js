// Compiles BHTEsettlement.sol with OpenZeppelin imports resolved from
// node_modules. Prints errors/warnings and exits non-zero on error.
const fs = require('fs');
const path = require('path');
const solc = require('solc');

const target = process.argv[2] || 'BHTEsettlement.sol';
const source = fs.readFileSync(path.join(__dirname, target), 'utf8');

function findImport(importPath) {
  const candidates = [
    path.join(__dirname, 'node_modules', importPath),
    path.join(__dirname, importPath),
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) {
      return { contents: fs.readFileSync(c, 'utf8') };
    }
  }
  return { error: 'File not found: ' + importPath };
}

const input = {
  language: 'Solidity',
  sources: { [target]: { content: source } },
  settings: {
    optimizer: { enabled: true, runs: 200 },
    outputSelection: { '*': { '*': ['abi', 'evm.bytecode.object'] } },
  },
};

const out = JSON.parse(solc.compile(JSON.stringify(input), { import: findImport }));

let hasError = false;
for (const e of out.errors || []) {
  console.log(e.formattedMessage);
  if (e.severity === 'error') hasError = true;
}

if (hasError) {
  console.log('COMPILE: FAILED');
  process.exit(1);
}

const contracts = out.contracts && out.contracts[target];
if (contracts) {
  for (const name of Object.keys(contracts)) {
    const bc = contracts[name].evm.bytecode.object;
    console.log(`COMPILE OK: ${name} (${bc.length / 2} bytes bytecode)`);
  }
}

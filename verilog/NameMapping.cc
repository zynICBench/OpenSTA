#include "VerilogReaderPvt.hh"
#include "NameMapping.hh"

namespace sta {
namespace NameResolve {

// ast access methods
bool 
Module::portdir(std::string const & port) const {
  return module->declaration(port.c_str())->direction()->isInput();
}

void
Module::connectPins(VerilogNet *netSeq, Module* submod) {

}

void 
Module::processModuleInst(VerilogModuleInst* s) {
  std::string instname = s->instanceName();
  std::string submodname  = s->moduleName();
  addInstSymbol(instname, submodname);
  Cell *cell = network->findAnyCell(submodname);
  Module *submod = ml->createModule(submodname, cell);
  VerilogNetSeq *netSeq = s->pins();
  if (netSeq) {
    for (auto & pin: *netSeq)
      connectPin(pin, submod);
  }
}

void
Module::processStmt(VerilogStmt* s) {
  if (s->isModuleInst())  { processModuleInst(s); }
  if (s->isLibertyInst()) { processLibertyInst(s); }
}

void
Module::processModule() {
  for (auto &s : *module->ports()) { addNetSymbol(s->name(), true); }
  for (auto &s : *module->stmts()) { processStmt(s); }
}

// fill methods
void 
Module::addInstSymbol(std::string const &instname, std::string const &modname) {
  Symbol &sym = addSymbol(instname);
  sym.isInst     = true;
  sym.isNet      = false;
  sym.name       = instname;
  sym.moduleName = modname;
}
void
Module::addNetSymbol(std::string const &netname, bool isPort) {
  Symbol &sym = addSymbol(netname);
  sym.isNet  = true;
  sym.isInst = false;
  sym.name   = netname;
  sym.isPort = isPort;
}
void
Module::addBusSymbol(std::string const &netname, bool isPort, int left, int right) {
  Symbol &sym = addSymbol(netname);
  sym.isNet  = true;
  sym.isInst = false;
  sym.name   = netname;
  sym.isPort = isPort;
  sym.range  = new Range(left, right);
}

// search methods
Module*
Module::instModule(std::string const & instname) const {
  std::string modname = symbols.find(instname)->second.moduleName;
  return modname == "" ? NULL : ml->getModule(modname);
}

Module::StringVec
Module::findHierSource(std::string const & key) {
  size_t pos = key.find('/');
  if (pos == -1) return findSource(key);
  std::string instname = key.substr(0, pos);
  std::string hiername = key.substr(pos + 1);
  Module* instmod = instModule(instname);
  if (!instmod) return StringVec(1, key);
  StringVec subress = instmod->findHierSource(hiername);
  StringVec finalRes;
  for (auto & subres : subress) {
    std::string res = instname + "/" + subres;
    finalRes.push_back(findOneSource(res));
  }
  return finalRes;
}

Module::StringVec
Module::findSource(std::string const & key) {
  Symbols::iterator it = symbols.find(key);
  if (it == symbols.end()) return StringVec(1, key);
  if (!it->second.isBus()) return StringVec(1, findOneSource(key));
  StringVec res;
  Range& range = *(it->second.range);
  range.resetpos();
  while (range.hasNext()) {
    std::stringstream ss; ss << key << "[" << range.curpos << "]";
    res.push_back(findOneSource(ss.str()));
    range.incrpos();
  }
  return res;
}

std::string 
Module::findOneSource(std::string const & key) {
  Symbols::iterator it = symbols.find(key);
  // if key is not in table
  if (it == symbols.end()) return key;
  assert(!it->second.isBus());
  // if (key not in current module step in)
  std::string res = it->second.src;
  if (res == "") {
    size_t pos = key.find('/');
    if (pos == -1) return key;  
    std::string instname = key.substr(0, pos);
    Module* instmod = instModule(instname);
    if (!instmod) return key;
    res = instname + "/" + instmod->findOneSource(key.substr(pos + 1));
    it->second.src = res;
  }
  res = findOneSource(res);
  it->second.src = res;  // update table
  return res;
};

Module*
ModuleList::createModule(std::string const &name, VerilogModule* m) {
  Modules::iterator it = modules.find(name);
  if (it != modules.end()) return it->second;
  return modules.insert({name, new Module(m, (ModuleList*)this)}).first->second;
}

} // end namespace NameResolve
} // end namespace sta
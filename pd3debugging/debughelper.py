
#ue4_executable_offset is the base address of the exe, seems to be different for each crash dump?

#ue4_executable_offset = 0x7FF5_9510_007D

ue4_executable_offset = 0x00007FF7F4B80000 - 0x0000000140000000
difference_threshold = 0x1000

class Symbol():
  def __init__(self, offset, name) -> None:
    self.offset = offset
    self.name = name
  
  def __str__(self):
    return hex(self.offset) + ":" + self.name

# formats a 64bit hex string with zeroes
def format_hex(str_):
  if len(str_) < 16:
    str = "0" * (16 - len(str_)) + str_
  return str

class SymbolMap():
  def __init__(self, path):
    self.symbols = []

    f = open(path, "r")
    self.process_map(f.read())
    f.close()

  def process_map(self, filecontents):
    for line in str.splitlines(filecontents):
      if line.startswith("#"): # comments
        continue

      if line == "":
        continue

      parts = line.split("|")
      offset = parts[0]
      name = parts[1]
      self.symbols.append(Symbol(int(offset, 16), name))

  def getFunctionSymbolFromOffset(self, offset):
    closest = Symbol(0xFFFF_FFFF_FFFF_FFFF, "Unknown")
    difference = 0xFFFF_FFFF_FFFF_FFFF
    for symbol in self.symbols:
      if abs(symbol.offset-offset) < difference:
        closest = symbol
        difference = abs(symbol.offset-offset)

    return (closest, difference)
  def getFunctionSymbolFromUE4StackTraceOffset(self, offset):
    return self.getFunctionSymbolFromOffset(offset - ue4_executable_offset)

def UE4Offset_String_To_ExeOffset_String(ue4offset):
  off = hex(int(ue4offset, 16) - ue4_executable_offset).replace("0x", "")
  off = format_hex(off)
  return off

stack = open("./vs2022stack.txt", "r")

symbolmap = SymbolMap("./debuggingmap.txt")

out = ""

for line in stack.readlines():
  line = line.replace("\n", "")
  parts = line.split("\t")
  module_and_offset = parts[1].replace("()", "")
  module = module_and_offset.split("!")[0]
  if "PAYDAY3" not in module:
    out += line + "\n"
    continue
  offset = int(module_and_offset.split("!")[1], 16)

  symbol, difference = symbolmap.getFunctionSymbolFromUE4StackTraceOffset(offset)
  
  if difference > difference_threshold: # function is not known
    str_off = UE4Offset_String_To_ExeOffset_String(module_and_offset.split('!')[1])
    out += f"\t{module}!{str_off}()\tUnknown" + "\n"
    #out += line + "\n"
    continue

  hex_offset = hex(symbol.offset).replace('0x', '')
  
  hex_offset = format_hex(hex_offset)

  out += f"\t{module}!{hex_offset}+{hex(difference)}()\t{symbol.name}+{hex(difference)} ({UE4Offset_String_To_ExeOffset_String(hex(offset))})" + "\n"

open("mapped_stack.txt", "w").write(out)
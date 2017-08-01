#!/usr/bin/env python2.7
from __future__ import print_function
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def process_file(filename, sym):
    print('Processing file:', filename)
    with open(filename, 'rb') as f:
        elffile = ELFFile(f)

        # Read the relocation sections from the file.
        # The section names are strings
        for section in elffile.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            print("\n\nRelocation section: %s" % section.name)
            symtab = elffile.get_section(section["sh_link"])

            for reloc in section.iter_relocations():
                symbol = symtab.get_symbol(reloc['r_info_sym'])
                # Some symbols have zero 'st_name', so instead what's used is
                # the name of the section they point at
                if symbol['st_name'] == 0:
                    symsec = elffile.get_section(symbol['st_shndx'])
                    symbol_name = symsec.name
                else:
                    symbol_name = symbol.name
                print('    Relocation (%s)' % 'RELA' if reloc.is_RELA() else 'REL')
                # Relocation entry attributes are available through item lookup
                print('      offset = 0x%x' % reloc['r_offset'])
                print('      sym = %s' % symbol_name)

if __name__ == '__main__':
    sym = ""
    i=1
    if sys.argv[i] == '--symbol':
        sym=sys.argv[i+1]
        i = i+2
    if sys.argv[i] == '--module':
        for filename in sys.argv[i+1:]:
            process_file(filename, sym)

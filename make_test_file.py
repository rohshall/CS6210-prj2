#!/usr/bin/env python
# encoding: utf-8

"""
Makes a dummy test file, each line is a new sector. Pass the number of
sectors desired as the only argument. Writes to stdout; redirect to the file of
your choice.

"""

import sys

sector_size = 512

def create_line_format(sector_count):
    """ Returns up the format for a line `sector_size` bytes long, padded with
    periods, and ending in "end." For each `sector_number`, do
        fmt.format(sect_num = sector_number)
    which will return the appropriate string
    """
    header = "Sector "
    content =  ": The quick brown fox jumped over the lazy dog."
    end ="end."
    num_digits = len(str(sector_count))
    char_count = sum([len(s) for s in (header, content, end)]) + num_digits
    dot_count = sector_size - char_count

    fmt = "{header}{{sect_num:0{digits}d}}{content}{dots}{end}" \
            .format(header=header,
                    digits=num_digits,
                    content=content,
                    dots="." *
                    dot_count,
                    end=end)
    return fmt

if __name__ == '__main__':
    usage = "usage: %s <sector_count>" % sys.argv[0]
    if len(sys.argv) < 2:
        print usage
        sys.exit(1)
    sector_count = int(sys.argv[1])
    fmt = create_line_format(sector_count)
    for i in range(sector_count):
        # use file.write() instead of printf to suppress newlines, spaces, etc
        sys.stdout.write(fmt.format(sect_num=i))
    sys.stdout.flush()

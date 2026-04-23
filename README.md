#     6502_emu is a 6502 emulator written in C++

A basic 6502 emulator I decided to develop in my free time to learn about CPU architecture and lower level workings of a system.
Done through reading documentation, end goal is to run age-appropriate code like MS-BASIC and complete compatibillity.
Planning to implement sound and screen capabillities.
</br></br>


# Building
A Makefile is included under /src. Building will create a /build folder containing the executable

    git clone https://github.com/ValKozz/6502_emu
    cd src
    make
    cd ../build
    ./6502_emu <rom-file>

Running <code>make clean</code> will remove the build folder and it's associated files.

</br>

## TODO
- Separate code in appropriate brances
- Implement full instruction set and all addressing modes associated with it (in progress)
- Fix Debug messages and create a comprehensive debug compilation mode for testing
- Implement a basic screen sub-module
- Implement a basic sound sub-module

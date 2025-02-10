DESCRIPTION

Very simple and rudimentary tool for handling ESC (EtherCAT Slave Controller) related data including:

- Defining EtherCAT devices and exporting them to ESI compliant XML files through simple Web UI

- Generating SII EEPROM binary for the ESC according to ETG.2010 from an ESI compliant XML (ETG.2000)

- Decode existing SII EEPROM binaries and (hopefully) print the contents in a human readable manner.

- Outputting OpenEtherCAT Society Slave Stack (SOES) compatible object dictionary and ESC configuration files.


The idea is that this tool can make handling simple EtherCAT units easier and more manageable.

Configuration file is saved as simple JSON.

Code structure is somewhat of a mess, and segmentation faults are for free...

BUILDING

Use git to clone third party modules, then use "cmake . && make".

USAGE

The main executable is called "esctool". When called with "-D" it starts in an "interactive" mode, where it serves a simple web page
on http://localhost:5001. This page can be used to handle the different devices, including exporting a defined device to XML, and subsequently to the binary SII EEPROM and SOES compatible object dictionary and configuration files. All files will end up in "<DeviceName>_out" folder.

Find example(s) in the examples folder.

(2025) Newtec A/S, Martin Hejnfelt
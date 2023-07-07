///////////////////////////////////////////////////////////////////////////////////
// Shared command line specs

let commandLineParser = require('./commandLineParser');

// Serial port options
let serial_port_specs = [
    {
        name: "<serialport>",
        help: "Serial port of target device"
    },
];

// Serial port packet layer options
let serial_port_packet_specs = [
    {
        name: "--packet-size:<n>",
        help: "Size of data chunks transmitted (default=4096)",
        default: 4096,
    },
    {
        name: "--packet-timeout:<n>",
        help: "Time out to receive packet ack in millis (default=300ms)",
        default: 300,
    },
    {
        name: "--ping-attempts:<n>",
        help: "How many times to ping for device before giving up (default=20)",
        default: 20,
    },
    {
        name: "--serial-log:<file>",
        default: null,
        help: "File to write low level log of serial comms"
    },
    {
        name: "--no-version-check",
        help: "Don't check bootloader version on device",
    },
];

// Additional serial port options when loading/uploading
let serial_port_load_specs = [
    {
        name: "--flash-baud:<n>|-b",
        help: "Baud rate for flashing (default=1000000)",
        default: 1000000,
    },
    {
        name: "--reset-timeout:<ms>",
        help: "How long device should wait for packet before resetting "
            + "to the default baud and CPU frequency (default=500ms)",
        parse: commandLineParser.parse_integer,
        default: 500,
    },
    {
        name: "--cpu-boost:[yes|no|auto]",
        help: "Whether to boost CPU clock frequency during uploads\n"
            + "auto = yes if flash baud rate > 1M",
        default: "auto",
    },
];

let common_specs = [
    {
        name: "--help",
        help: "Show this help",
        terminal: false,
    },
]


module.exports = {
    serial_port_specs,
    serial_port_packet_specs,
    serial_port_load_specs,
    common_specs,
}




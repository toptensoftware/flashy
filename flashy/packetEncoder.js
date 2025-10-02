///////////////////////////////////////////////////////////////////////////////////
// Packet encoding and decoding
//
// Packet format
// =============
//
// 0xAA 0xAA 0xAA        - start of packet signal bytes
// 0x00                  - separator byte
// [sequenceNumber]      - var length encoded sequence number
// [command]             - var length encoded command id
// [length]              - var length encoded data length
// [data bytes]          - data bytes
// 0xNNNNNNNN            - CRC32 of all data from separator byte to here (big endian)
// 0xFF                  - packet terminator byte
//
// All data in the packet body (separator to CRC inclusive) is encoded 
// such that a sequence of 2x 0xAA bytes is always followed by a stuffing
// byte of 0x00.  This escapes embedded 0xAA 0xAA 0xAA from incorrectly
// triggering a new packet signal. Stuffing bytes are included in the CRC
//
// Variable length encoded fields are encoded using MIDI style variable
// length encoding (7 bits per byte, MSB first, non-LSB bytes flagged with
// top bit set ie: |= 0x80).

import crc32 from './crc32.js';

// Special bytes
const signal_byte = 0xAA;
const separator_byte = 0;
const stuff_byte = 0;
const terminator_byte = 0x55;

// Helper to variable length encode a value
function var_len_enc(value, callback, bit)
{
    if (value > 127)
    {
        var_len_enc(value >> 7, callback, 0x80);
    }
    callback((value & 0x7F) | bit);
}

// Encode a single packet of data
function packet_encode(callback, seq, cmd, buf)
{
    // Write signal
    callback(signal_byte);
    callback(signal_byte);
    callback(signal_byte);

    // Setup context
    let crc = crc32.start();
    let signal_bytes = 0;

    let buflen = buf && buf.length;

    // Write header
    write(separator_byte);
    var_len_enc(seq, write);
    var_len_enc(cmd, write);
    var_len_enc(buflen, write);

    // Write data
    for (let i = 0; i < buflen; i++)
    {
        write(buf[i]);
    }

    // Write crc
    let crcResult = crc32.finish(crc);
    write((crcResult >>> 24) & 0xFF);
    write((crcResult >>> 16) & 0xFF);
    write((crcResult >>> 8) & 0xFF);
    write((crcResult >>> 0) & 0xFF);

    // Write terminator
    callback(terminator_byte);

    // Helper to write a byte of data to output stream, inserting
    // stuffing bytes and calculating CRC
    function write(data)
    {
        callback(data);
        crc = crc32.update(crc, data);

        if (data == signal_byte)
        {
            if (signal_bytes == 1)
            {
                // Write stuffing byte (0)
                data = stuff_byte;
                crc = crc32.update(crc, data);
                callback(data);
                signal_bytes = 0;
            }
            else
            {
                signal_bytes++;
            }
        }
        else
            signal_bytes = 0;
    }
}

// Packet decoder
// callback - a function(seq, cmd, buf) to be called with decoded packets
// error - a function(mgs) to be called with error messages
// maxlength - max length of a data packet (to prevent over allocating memory
//             on receipt of a bad packet before it can be validated via CRC)
// Returns - a function(byte) that should be called with individual data stream
//           bytes.
function packet_decoder(callback, error, maxlength)
{
    maxlength = maxlength || 1024;

    let state = "waiting_signal";
    let signal_bytes_seen = 0;
    let buf = Buffer.alloc(maxlength);
    let count;
    let cmd;
    let seq;
    let length=0;
    let crcRecv;
    let crcCalc;

    function flush_intermediate_data()
    {
        if (length > 0)
        {
            //console.log(`intermediate data: '${buf.subarray(0, length)}'`);
            length = 0;
        }
    }   



    return function receive_byte(data)
    {
        //process.stdout.write(`0x${data.toString(16)}, `)
        // Monitoring for signal happens even while decoding packets
        // and if seen discards the current packet and starts a new one
        if (data == signal_byte)
        {
            if (signal_bytes_seen < 3)
                signal_bytes_seen++;
            if (signal_bytes_seen == 3)
            {
                // In middle of something else?
                if (state != "waiting_signal")
                {
                    error && error("packet discarded: new packet signal detected");
                }

                flush_intermediate_data();

                // Start new packet
                state = "expect_separator";
                count = 0;
                return;
            }
        }
        else
        {
            // Ignore zero byte after two consecutive signal_bytes
            if (signal_bytes_seen == 2)
            {
                signal_bytes_seen = 0;

                if (data != stuff_byte)
                {
                    state = "waiting_signal";
                    length = 0;
                }
                else if (state != "expect_crc" || count == 0)
                    crcCalc = crc32.update(crcCalc, data);
                return;
            }

            signal_bytes_seen = 0;
        }

        switch (state)
        {
        case "waiting_signal":
            // Still waiting for signal
            if (data != signal_byte)
            {
                if (length >= buf.length)
                {
                    flush_intermediate_data();
                }
                buf[length++] = data;
            }
            break;

        case "expect_separator":
            if (data != separator_byte)
            {
                error && error(`discarded packet: expected separator byte, received 0x${data.toString(16)}`);
                state = "waiting_signal";
                length = 0;
            }
            else
            {
                state = "expect_seq";
                cmd = 0;
                seq = 0;
                length = 0;
                crcCalc = crc32.start();
                crcCalc = crc32.update(crcCalc, data);
            }
            break;

        case "expect_seq":
            crcCalc = crc32.update(crcCalc, data);
            seq = ((seq << 7) | (data & 0x7f)) >>> 0;
            if ((data & 0x80) == 0)
            {
                state = "expect_cmd";
            }
            break;

        case "expect_cmd":
            crcCalc = crc32.update(crcCalc, data);
            cmd = ((cmd << 7) | (data & 0x7f)) >>> 0;
            if ((data & 0x80) == 0)
            {
                state = "expect_length";
            }
            break;
    
        case "expect_length":
            crcCalc = crc32.update(crcCalc, data);
            length = ((length << 7) | (data & 0x7f)) >>> 0;
            if ((data & 0x80) == 0)
            {
                if (length > buf.length)
                {
                    error && error(`discarded packet: length exceeded limit (${length} > ${buf.length})`);
                    state = "waiting_signal";
                    length = 0;
                }
                else
                {
                    state = length == 0 ? "expect_crc" : "expect_data";
                    count = 0;
                }
            }
            break;

        case "expect_data":
            crcCalc = crc32.update(crcCalc, data);
            buf[count] = data;
            count++;
            if (count == length)
            {
                state = "expect_crc";
                count = 0;
            }
            break;

        case "expect_crc":
            crcRecv = (crcRecv << 8 | data) >>> 0;
            count++;
            if (count == 4)
            {
                crcCalc = crc32.finish(crcCalc);
                if (crcCalc != crcRecv)
                {
                    error && error(`discarded packet: checksum mismatch (recv: 0x${crcRecv.toString(16)} expected: 0x${crcCalc.toString(16)})`);
                    state = "waiting_signal";
                    length = 0;
                }
                else
                    state = "expect_terminator";
            }
            break;

        case "expect_terminator":
            if (data == terminator_byte)
            {
                callback(seq, cmd, buf.subarray(0, length));
            }
            else
            {
                error && error(`discarded packet: missing terminator byte`);
            }
            state = "waiting_signal";
            length = 0;
            break;
        }
    }
}

export default {
    encode: packet_encode,
    decode: packet_decoder,
}
///////////////////////////////////////////////////////////////////////////////////
// Utilies for parsing and rechunking Intel HEX files.

import fs from 'node:fs';

function parse_nibble(ch)
{
    if (ch >= 48 && ch <= 57)     // 0-9
        return ch - 48;
    if (ch >= 65 && ch <= 70)     // A-F
        return ch - 65 + 0xA;
    if (ch >= 97 && ch <= 102)    // a-f
        return ch - 97 + 0xA;
    return -1;
}

// Parses an intel .HEX file into records
// Returns an object with read() and close() methods
// read() returns a object { type: "00", addr: <int>, data: <buf> }
//              where "00" is the record type.
//        can also return { type: "error", message: <string> }
//                    and { type: "eof" }
function parser(hexFile)
{
    // Open file
    let fd = fs.openSync(hexFile, `r`);

    // State machine
    let char = 0;
    let state = "init";
    let next_state = null;
    let reclen = 0;
    let recaddr = 0;
    let parsed_byte = 0;
    let checksum = 0;

    // Buffer for record data
    let codebuf = Buffer.alloc(256);
    let codebuf_used = 0;

    function parse_byte(nextState)
    {
        parsed_byte = 0;
        state = "parse_byte";
        next_state = nextState;
    }

    function read()
    {   
        while (true)
        {
            switch (state)
            {
                case "init":
                    if (char == 58)         // ':'
                    {
                        checksum = 0;
                        char = read_char();
                        parse_byte("reclen");
                        continue;
                    }
                    else if (char < 0)
                    {
                        return null;
                    }

                    // Ignore everything else
                    char = read_char();
                    break;
    
                case "reclen":
                    reclen = parsed_byte;
                    parse_byte("recaddr_hi");
                    break;
    
                case "recaddr_hi":
                    recaddr = parsed_byte << 8;
                    parse_byte("recaddr_lo");
                    break;

                case "recaddr_lo":
                    recaddr |= parsed_byte;
                    parse_byte("rectype");
                    break;
    
                case "rectype":
                    rectype = parsed_byte;
                    codebuf_used = 0;
                    if (reclen == 0)
                    {
                        parse_byte("checksum");
                    }
                    else
                    {
                        parse_byte("data");
                    }
                    break;
    
                case "data":
                    codebuf[codebuf_used++] = parsed_byte;
                    if (codebuf_used == reclen)
                    {
                        parse_byte("checksum");
                    }
                    else
                    {
                        parse_byte("data");
                    }
                    break;
    
                case "checksum":
                    // Check the check sum
                    if (parsed_byte != (((checksum ^ 0xFF) + 1) & 0xFF))
                        return { 
                            type: "error",
                            message: `checksum error`,
                        }
                        
                    // Setup for next record
                    state = "init";
    
                    // Return record
                    return {
                        type: ("0" + rectype.toString(16)).slice(-2),
                        addr: recaddr,
                        data: codebuf.subarray(0, reclen),
                    }
    
                case "parse_byte":
                case "parse_byte_lo":
                    let nib = parse_nibble(char);
                    if (nib >= 0)
                    {
                        char = read_char();
                        if (state == "parse_byte")
                        {
                            parsed_byte = nib << 4;
                            state = "parse_byte_lo";
                        }
                        else
                        {
                            parsed_byte |= nib;
                            if (next_state != 'checksum')
                                checksum = (checksum + parsed_byte) & 0xFF;
                            state = next_state;
                        }
                    }
                    else if (char == 20 || char == 13 || char == 10 || char == 9)
                    {
                        // White space, ignore
                        char = read_char();
                    }
                    else
                    {
                        return { 
                            type: "error", 
                            message: `invalid hex data, unexpected character '${String.fromCharCode(char)}'` 
                        }
                    }
                    break;
    
            }
        }
    }

    // Close the file
    function close()
    {
        // Close
        fs.closeSync(fd);
    }

    // Read buffer
    let readbuf = Buffer.alloc(4096);
    let readbuf_used = 0;
    let readbuf_pos = 0;
    
    // Read next character from file
    function read_char()
    {
        // Read more?
        if (readbuf_pos >= readbuf_used)
        {
            // Read bytes
            readbuf_used = fs.readSync(fd, readbuf, 0, readbuf.length);
            if (readbuf_used == 0)
                return -1;
            readbuf_pos = 0;
        }

        // Return next character
        return readbuf[readbuf_pos++];
    }

    // Read first character
    char = read_char();
    return {
        read,
        close,
    }
}

// Chunks '00' records from an Intel Hex parser into different sized records.
// API is the same to use as parser.
// Params:
//   parser - the underlying parser to read from
//   max_chunk_size - the maximum size of coalesced chunks (including header)
//   header_size - an optional number of bytes to reserve at the start of each
//                 chunk's buffer
// If set, header_size reserves space at the start of the output buffers start for 
// user defined header information. This can be used to save copying the resulting 
// data buffer to another buffer for encoding and transmission.
function chunker(parser, max_chunk_size, header_size)
{
    header_size = header_size || 0;

    // Buffer to coalesc records into
    let buf = Buffer.alloc(max_chunk_size);
    let bufUsed = 0;
    let bufAddr = 0;
    let bufAvail = max_chunk_size - header_size;

    // Read first record
    let r = null;

    // Helper to flush the buffer
    function flush_buffer()
    {
        let temp = {
            type: "00",
            addr: bufAddr,
            data: buf.subarray(0, header_size + bufUsed),
        }
        bufUsed = 0;
        return temp;
    }

    function read()
    {
        while (true)
        {
            if (r == null)
                r = parser.read();
            if (r == null)
                return null;

            if (r.type == '00')
            {
                // Can we combine with previous?
                if (bufUsed != 0 && bufAddr + bufUsed != r.addr)
                {
                    return flush_buffer();
                }

                // Setup coalesced buffer address
                if (bufUsed == 0)
                    bufAddr = r.addr;

                // Copy record into our buffer
                let room = bufAvail - bufUsed;
                let copy = Math.min(room, r.data.length);
                r.data.copy(buf, header_size + bufUsed, 0, copy);
                bufUsed += copy;

                if (copy < r.data.length)
                {
                    // Truncate current record
                    r.addr += copy;
                    r.data = r.data.subarray(copy, r.data.length);
                }
                else
                {
                    r = null;
                }

                // Buffer full?
                if (bufUsed >= bufAvail)
                {
                    return flush_buffer();
                }
            }
            else
            {
                if (bufUsed != 0)
                    return flush_buffer();
                
                let temp = r;
                r = null;
                return temp;
            }
        }
    }

    function close()
    {
        parser.close();
    }

    return { 
        read, 
        close 
    }
}

export default { parser, chunker };
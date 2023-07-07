let commandLineSpecs = require('./commandLineSpecs');
let struct = require('./struct');

const PACKET_ID_READDIR = 6;


let direntry = 
{
    name: "direntry",
    fields: 
    [
        "uint32le size",
        "uint32le time",
        "uint8    attr",
        "string   name",
    ]
}

let dirlist = 
{
    name: "dirlist",
    fields: 
    [
        "uint32le error",
        "uint32le token",
        "uint32le $count",
        "direntry[$count] entries",
    ],
}

let lib = struct.library();
lib.defineType(direntry);
lib.defineType(dirlist);

function formatAttr(attr)
{
    return `${(attr & 0x01) ? 'r' : '-'}${(attr & 0x02) ? 'h' : '-'}${(attr & 0x04) ? 's' : '-'}${(attr & 0x10) ? 'd' : '-'}${(attr & 0x20) ? 'a' : '-'}`;
}

function unpackDosTime(time)
{

    return {
        year: ((time >>> 25) & 0x7f) + 1980,
        month: ((time >>> 21) & 0x0f),
        date: ((time >>> 16) & 0x1f),
        hours: ((time >>> 11) & 0x1f),
        minutes: ((time >>> 5) & 0x3f),
        seconds: ((time >>> 0) & 0x1f) * 2
    }
}

function formatTime(time)
{
    let d = unpackDosTime(time);
    let r =  d.date.toString().padStart(2, '0') 
           + "/" + d.month.toString().padStart(2, '0')
           + "/" + d.year.toString().padStart(2, '0')
           + " " + (((d.hours + 11) % 12) + 1).toString().padStart(2, '0')
           + ":" + d.minutes.toString().padStart(2, '0')
           + " " + (d.hours >= 12 ? "PM" : "AM");
    return r;
}

async function run(ctx)
{
    let port = ctx.port;
    let cl = ctx.cl;

    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);

    // Create data packet
    let readdir = Buffer.alloc(4096);
    readdir.writeUint16LE(0, 0);                                    // Token
    let length = 4 + readdir.write(ctx.cl.filespec, 4, "utf8");     // Filespec
    readdir[length] = 0;

    while (true)
    {
        // Send read dir request
        let r = await ctx.layer.send(PACKET_ID_READDIR, readdir.subarray(0, length + 1));
        
        // Check error before decoding
        let err = r.readUInt32LE(0) != 0;
        if (err)
        throw new Error(`Failed listing directory ${err}`);
        
        // List it
        let dir = lib.decode("dirlist", r);
        for (let e of dir.entries)
        {
            console.log(`${formatAttr(e.attr)} ${formatTime(e.time)}  ${(e.attr & 0x10) ? "<DIR>".padEnd(14, ' ') : e.size.toLocaleString().padStart(14, ' ')} ${e.name}`);
        }    

        if (dir.token == 0)
            break;
        
        readdir.writeUint32LE(dir.token, 0);                                    // Token
    }
}
    
module.exports = {
    synopsis: "Display device bootloader status",
    spec: [
        ...commandLineSpecs.serial_port_specs,
        {
            name: "<filespec>",
            help: "The directory and/or files to list",
            default: "/",
        },
        ...commandLineSpecs.serial_port_packet_specs,
        ...commandLineSpecs.common_specs,
    ],
    run,
}
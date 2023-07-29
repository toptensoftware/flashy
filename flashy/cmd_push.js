let fs = require('fs');
let path = require('path');

async function push_file(ctx, local_path, remote_path)
{
    if (ctx.cl.verbose)
        console.log(`pushing file: ${local_path} => ${remote_path}`);

    // Get local file stat
    let stat = fs.statSync(local_path);

    let offset = 0;
    let buf = Buffer.alloc(ctx.layer.options.max_packet_size);
    let token = Date.now() & 0x7FFFFFFF;

    // Open the file
    let fd = fs.openSync(local_path, "r");

    process.stdout.write(`${remote_path}: `)

    // Send data
    while (true)
    {
        // Setup packet
        buf.writeUInt32LE(token, 0);
        buf.writeUInt32LE(offset, 4);
        let bytes_read = fs.readSync(fd, buf, 8, buf.length - 8, offset);
        if (bytes_read == 0)
            break;

        // Send it
        let r = await ctx.layer.sendPushData(buf.subarray(0, bytes_read + 8));
        let err = r.readUInt32LE(0);
        if (err)
            throw new Error(`failed to push file data (err: ${err})`);

        process.stdout.write('.');

        // Update offset
        offset += bytes_read;
    }

    // Close file
    fs.closeSync(fd);

    // Setup commit
    let commit = {
        token,
        size: offset,
        date: ((stat.mtime.getFullYear() - 1980) << 9) | ((stat.mtime.getMonth() + 1) << 5) | (stat.mtime.getDate() << 0),
        time: (stat.mtime.getHours() << 11) | (stat.mtime.getMinutes() << 5) | (stat.mtime.getSeconds() / 2),
        attr: 0x20,             // AM_ARC
        overwrite: !ctx.cl.noClobber,
        name: remote_path,
    }

    // Commit it
    let r = await ctx.layer.sendPushCommit(commit);
    let err = r.readUInt32LE(0);
    if (err)
    {
        if (err == 8)   // FR_EXIST
            throw new Error(`file already exists`);
        else
            throw new Error(`failed to commit pushed file (err: ${err})`);
    }

    // Done!
    process.stdout.write(' ok\n');
}


async function run(ctx)
{
    // Wait for device
    await ctx.port.switchBaud(115200);
    await ctx.layer.ping(ctx.cl.verbose);
    await ctx.layer.boost(ctx.cl);

    for (let f of ctx.cl.files)
    {
        await push_file(ctx, f, f);
    }
}

module.exports = {
    synopsis: "Copies files to the device",
    spec: [
        {
            name: "<files>",
            help: "The files to push",
            default: [],
        },
        {
            name: "--cwd:<dir>",
            help: "The working directory on the device in which to execute the command (default = /)",
            default: '/',
        },
        {
            name: "--to:<target>",
            help: "Where to save the pulled file(s)",
            default: '.',
        },
        {
            name: "--no-clobber|-n",
            help: "Don't overwrite existing files",
            default: false,
        }
    ],
    run,
}
let fs = require('fs');
let path = require('path');

let struct = require('./struct');
let argUtils = require('./argUtils');

let lib = struct.library();
lib.defineType({
    name: "pull_header",
    fields: [
        "uint32le size",
        "uint16le time",
        "uint16le date",
        "uint8 attr",
        "string filename",
    ]
});


async function pull_file(ctx, remote_path, local_path, timestamp)
{
    if (ctx.cl.verbose)
        console.log(`pulling file: ${remote_path} => ${local_path}`);

    let header;
    let fd;
    let expected_offset = 0;

    // Open the file
    fd = fs.openSync(local_path, ctx.cl.noClobber ? "wx" : "w");

    // Handler for the file header
    function onHeader(buf)
    {
        // Decode header
        header = lib.decode("pull_header", buf);
        process.stdout.write(`${remote_path}: `)
    }

    // Handler for data
    function onData(buf)
    {
        process.stdout.write('.');
        // Write file
        let offset = buf.readUInt32LE(0);
        if (expected_offset != offset)
            throw new Error("File packet offset mistmatch");
        expected_offset += fs.writeSync(fd, buf.slice(4), 0);
    }

    // Make request
    let r = await ctx.layer.sendPull(remote_path, {
        onHeader,
        onData,
    });

    // Close file
    if (fd)
    {
        fs.futimesSync(fd, timestamp, timestamp);
        fs.closeSync(fd);
    }

    // Check result
    let err = r.readUInt32LE(0);
    if (err)
    {
        if (fs.existsSync(local_path))
            fs.unlinkSync(local_path);
        throw new Error(`failed to pull file (err: ${err})`);
    }

    process.stdout.write(' ok\n');

}


async function pull_dir(ctx, remote_path, local_path)
{
    if (ctx.cl.verbose)
        console.log(`pulling directory: ${remote_path} => ${local_path}`)

    // Read directory
    let entries = await ctx.layer.exec_ls(ctx.cl.rwd, `ls -al ${remote_path}`);

    // Create the target directory
    if (!fs.existsSync(local_path))
        fs.mkdirSync(local_path, {recursive: true});
    
    // Copy files
    for (let e of entries)
    {
        if (!e.isdir)
        {
            await pull_file(ctx, 
                argUtils.pathjoin(remote_path, e.name),
                path.join(local_path, e.name),
                e.mtime,
                );
        }
        else
        {
            await pull_dir(ctx,
                argUtils.pathjoin(remote_path, e.name),
                path.join(local_path, e.name)
                );
        }
    }
}

async function run(ctx)
{
    // Wait for device
    await ctx.port.switchBaud(115200);
    await ctx.layer.ping(ctx.cl.verbose);
    await ctx.layer.boost(ctx.cl);

    // Read directory entries for specified files
    let entries = await ctx.layer.exec_ls(ctx.cl.rwd, `ls -ald ${ctx.cl.files.join(" ")}`);

    // Quit if nothing to do
    if (entries.length == 0)
    {
        return 0;
    }

    // Stat the target
    let toStat;
    try
    {
        toStat = fs.statSync(ctx.cl.to);
    }
    catch
    {
        toStat = null;
    }

    // Is it a single file
    if (entries.length == 1 && !entries[0].isdir)
    {
        if (toStat != null && toStat.isDirectory())
        {
            // Single file to directory
            return await pull_file(ctx, 
                argUtils.pathjoin(ctx.cl.rwd, entries[0].name), 
                path.join(ctx.cl.to, path.basename(entries[0].name)),
                entries[0].mtime
                )
        }
        else
        {
            // Single file to a different file name
            return await pull_file(ctx,
                argUtils.pathjoin(ctx.cl.rwd, entries[0].name), 
                ctx.cl.to,
                entries[0].mtime
                )
        }
    }

    // Target must be a directory
    if (toStat == null)
        throw new Error(`target directory '${ctx.cl.to}' not found`);
    if (!toStat.isDirectory())
        throw new Error(`target '${ctx.cl.to}' must be a directory when pulling multiple files`);

    // Pull files
    for (let e of entries)
    {
        if (!e.isdir)
        {
            await pull_file(ctx, 
                argUtils.pathjoin(ctx.cl.rwd, e.name), 
                path.join(ctx.cl.to, path.basename(e.name)),
                e.mtime
                );
        }
        else
        {
            await pull_dir(ctx,
                argUtils.pathjoin(ctx.cl.rwd, e.name), 
                path.join(ctx.cl.to, path.basename(e.name))
                );
        }
    }
}

module.exports = {
    synopsis: "Copies files from the device",
    spec: [
        {
            name: "<files>",
            help: "The files to pull",
            default: [],
        },
        {
            name: "--rwd:<dir>",
            help: "The remote working directory (ie: on the device) in which to execute the command (default = /)",
            default: '/sd',
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
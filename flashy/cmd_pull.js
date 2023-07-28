let fs = require('fs');
let path = require('path');

let struct = require('./struct');
const exp = require('constants');

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

function pathjoin(a, b)
{
    // Convert to forward slashes
    a = a.replace(/\\/g, '/');
    b = b.replace(/\\/g, '/');

    // Is b fully qualified?
    if (b.match("/^[a-zA-Z0-9]+\:") || b.startsWith('/'))
        return b;

    if (!a.endsWith('/'))
        return a + '/' + b;
    else
        return a + b;
}

// Parse the output of a "ls -l" output from device
function parse_ls_output(output)
{
    let entries = [];
    for (let l of output.split('\n'))
    {
        let m = l.match(/^([-d][-r][-s][-h][-a])\s+(\d\d)\/(\d\d)\/(\d\d\d\d)\s+(\d\d)\:(\d\d)\:(\d\d)\s+(\d+)\s+(.*)$/);
        if (m)
        {
            entries.push({
                attr: m[1],
                mtime: new Date(parseInt(m[4]), parseInt(m[3])-1, parseInt(m[2]), parseInt(m[5]), parseInt(m[6]), parseInt(m[7])),
                size: m[8],
                name: m[9],
                isdir: m[1][0] == 'd',
            })
        }
    }

    return entries;

}

async function exec_ls(ctx, cmd)
{
    let bufs = [];
    function onStdOut(data)
    {
        bufs.push(Buffer.concat([data]));
    }

    function onStdErr(data)
    {
        process.stderr.write(data);
    }

    let r = await ctx.layer.sendCommand(ctx.cl.cwd, cmd, {
        onStdOut,
        onStdErr,
    })

    if (r.exitCode != 0)
        throw new Error(`Failed to list files '${cmd}'`);

    // Parse directory listing
    return parse_ls_output(Buffer.concat(bufs).toString('utf8'));
}


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
    let entries = await exec_ls(ctx, `ls -al ${remote_path}`);

    // Create the target directory
    if (!fs.existsSync(local_path))
        fs.mkdirSync(local_path, {recursive: true});
    
    // Copy files
    for (let e of entries)
    {
        if (!e.isdir)
        {
            await pull_file(ctx, 
                pathjoin(remote_path, e.name),
                path.join(local_path, e.name),
                e.mtime,
                );
        }
        else
        {
            await pull_dir(ctx,
                pathjoin(remote_path, e.name),
                path.join(local_path, e.name)
                );
        }
    }
}

async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(ctx.cl.verbose);
    await ctx.layer.boost(ctx.cl);

    // Read directory entries for specified files
    let entries = await exec_ls(ctx, `ls -ald ${ctx.cl.files.join(" ")}`);

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
                pathjoin(ctx.cl.cwd, entries[0].name), 
                path.join(ctx.cl.to, path.basename(entries[0].name)),
                entries[0].mtime
                )
        }
        else
        {
            // Single file to a different file name
            return await pull_file(ctx,
                pathjoin(ctx.cl.cwd, entries[0].name), 
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
                pathjoin(ctx.cl.cwd, e.name), 
                path.join(ctx.cl.to, path.basename(e.name)),
                e.mtime
                );
        }
        else
        {
            await pull_dir(ctx,
                pathjoin(ctx.cl.cwd, e.name), 
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
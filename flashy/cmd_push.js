import path from 'node:path';
import fs from 'node:fs';
import argUtils from './argUtils.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

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

    process.stdout.write(`${local_path}: `)

    // Send data
    while (true)
    {
        // Setup packet
        buf.writeUInt32LE(token, 0);
        buf.writeUInt32LE(offset, 4);
        let bytes_read = fs.readSync(fd, buf, 8, buf.length - 8, offset);

        // Send it
        let r = await ctx.layer.sendPushData(buf.subarray(0, bytes_read + 8));
        let err = r.readUInt32LE(0);
        if (err)
            throw new Error(`failed to push file data (err: ${err})`);

        process.stdout.write('.');

        // Update offset
        offset += bytes_read;

        if (bytes_read < buf.length - 8)
            break;
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

async function push_dir(ctx, local_path, remote_path)
{
    if (ctx.cl.verbose)
        console.log(`pushing directory: ${local_path} => ${remote_path}`);

    let dir = fs.opendirSync(local_path);
    try
    {
        while (true)
        {
            let de = dir.readSync();
            if (de == null)
                break;
            
            if (de.isDirectory())
            {
                await push_dir(ctx, path.join(local_path, de.name), argUtils.pathjoin(remote_path, de.name));
            }
            else
            {
                await push_file(ctx, path.join(local_path, de.name), argUtils.pathjoin(remote_path, de.name));
            }
        }
    }
    finally
    {
        dir.closeSync();
    }
}

async function run(ctx)
{
    // Wait for device
    await ctx.port.switchBaud(115200);
    await ctx.layer.ping(ctx.cl.verbose);
    await ctx.layer.boost(ctx.cl);

    if (ctx.cl.bootloader)
    {
        ctx.cl.files.push(path.join(__dirname, "../bootloader_images", "kernel*.img"))
    }

    // Expand files
    let files = argUtils.expandArgs(ctx.cl.files);
    
    // Quit if any specified files are missing
    let missing = files.filter(x => x.stat == null);
    if (missing.length)
    {
        throw new Error(`no such file or directory: ${missing[0].relative}`);
    }

    // Work out target
    let target = argUtils.pathjoin(ctx.cl.rwd, ctx.cl.to);
    let target_stats = await ctx.layer.exec_ls('/', `ls -ald \"${target}\"`, true);
    if (target_stats.length > 1)
    {
        throw new Error(`target '${target}' matches multiple items`);
    }
    let target_stat = target_stats.length > 0 ? target_stats[0] : null;
    
    // Single file?
    if (files.length == 1 && !files[0].stat.isDirectory())
    {
        if (target_stat != null && target_stat.isdir)
        {
            // Target is a directory
            await push_file(ctx, files[0].relative, argUtils.pathjoin(target, path.basename(files[0].relative)));
        }
        else
        {
            // Target is a file
            await push_file(ctx, files[0].relative, target);
        }
        return;
    }

    // Check target exists and is a directory
    if (target_stat == null)
        throw new Error(`target directory '${target}' doesn't exist`);
    if (!target_stat.isdir)
        throw new Error(`target '${target}' is not a directory`);

    // Expand list of files and process
    for (let f of files)
    {
        let remote_path = argUtils.pathjoin(target, path.basename(f.relative))
        if (f.stat.isDirectory())
            await push_dir(ctx, f.relative, remote_path);
        else
            await push_file(ctx, f.relative, remote_path);
    }
}

export default {
    synopsis: "Copies files to the device",
    spec: [
        {
            name: "<files>",
            help: "The files to push",
            default: [],
        },
        {
            name: "--bootloader",
            help: "Push the bootloader images",
        },
        {
            name: "--to:<target>",
            help: "Where to place the pushed file(s)",
            default: '.',
        },
        {
            name: "--rwd:<dir>",
            help: "The remote working directory (ie: on the device) in which to execute the command (default = /)",
            default: '/sd',
        },
        {
            name: "--no-clobber|-n",
            help: "Don't overwrite existing files",
            default: false,
        }
    ],
    run,
}
import child_process from 'node:child_process';
import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Run a command, return stdout
function run(cmd)
{
    return child_process.execSync(cmd).toString('utf8');
}

// Check if running under wsl2
function isWsl2()
{
    if (os.platform != 'linux')
        return false;
    return run('uname -a').indexOf("WSL2") > 0;
}

// Find the host version of flashy by running "WHERE.EXE flashy"
function findWslHostFlashy()
{
    // Run WHERE on host to find flashy
    let locs = child_process.execSync("WHERE.EXE flashy").toString()
        .split('\n')
        .map(x => x.trim())
        .filter(x => x.endsWith('.cmd'));

    // Found?
    if (locs.length == 0)
        return null;
    else 
        return locs[0];
}

// Relaunch self using the host version of flashy
function runSelfUnderWindows()
{
    // Find host version of flashy
    let cmd = findWslHostFlashy();
    if (cmd == null)
    {
        process.stderr.write("It looks like you're trying to use Flashy in a WSL 2 environment with\n");
        process.stderr.write("a serial port.  This isn't supported by WSL2 but if you install Flashy\n");
        process.stderr.write("on the host Windows machine, I'll find it and relaunch myself there.\n\n")
        return;
    }

    // Find current directory as UNC path
    let uncHere =  run("wslpath -w .").trim();
    let ver = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8").version;


    // Note the CWD trickery below is to prevent CMD.EXE spitting out a warning about
    // UNC paths not supported as the current directory.  Change current directory
    // to C drive and then tell self to change directory back again once launched with 
    // the --cwd arg

    // Respawn self
    let r = child_process.spawnSync(
        "cmd.exe",
        [ "/c", cmd, `--cwd:${uncHere}`, `--ensure-version:${ver}`, ...process.argv.slice(2)],
        {
            stdio: 'inherit', 
            shell: false,
            cwd: "/mnt/c",
        }
    );
    
    // Quit with exit code of the child process
    process.exit(r.status);
}    

export default {
    isWsl2,
    runSelfUnderWindows,
}
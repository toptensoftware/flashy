let nodeRepl = require('node:repl');
let chalk = require('chalk');

// Find the position in a command line string of the last argument
function findLastArg(line)
{
    let i = 0;
    let len = line.length;
    let lastArgPos = 0;
    let pendingQuote = null;
    while (i < len)
    {
        switch (line[i])
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                i++;
                lastArgPos = i;
                break;

            case '\\':
                i+=2;
                break;
                
            case '\"':
                i++;
                while (i < len && line[i] != '\"')
                    i++;
                if (i<len)
                    i++;
                else
                    pendingQuote = '\"';
                break;
        
            case '\'':
                i++;
                while (i < len && line[i] != '\'')
                    i++;
                if (i<len)
                    i++;
                else
                    pendingQuote = '\'';
                break;

            case '&':
            case '|':
            case ';':
            case '(':
            case ')':
                i++;
                lastArgPos = i;
                return;

            default:
                i++;
                break;
        }
    }
    return { 
        lastArgPos,
        pendingQuote
    }
}

async function run(ctx)
{
    // Wait for device
    await ctx.port.switchBaud(115200);
    let ping = await ctx.layer.ping(ctx.cl.verbose);
    //await ctx.layer.boost(ctx.cl);

    // Get the label of the drive
    let vol;
    async function getLabel()
    {
        // get volume label
        let match = (await ctx.layer.exec_cmd("/", "label /sd")).match(/^(.*) ....-.... (.*)/);
        vol = match ? match[2] : "";
    }
    await getLabel();


    // The current remote working directory
    let rwd = '/sd'

    // The repl server instance
    let repl;

    function format_prompt()
    {
        return chalk.green(`${ping.model.name} (aarch${ping.aarch}) (${vol})`) + " " + chalk.cyan(rwd) + "> ";
    }

    // Command handler
    async function invoke_command(cmd, context, filename, callback) {

        if (cmd.trim().length == 0)
        {
            callback();
            return;
        }

        // Invoke it
        let r = await ctx.layer.sendCommand(rwd, cmd.trim(), null);

        // Save new working directory
        rwd = r.cwd;

        if (r.did_exit)
        {
            repl.close();
            return;
        }

        if (cmd.indexOf("label") >= 0)
        {
            await getLabel();
        }

        // Update prompt with new directry
        repl.setPrompt(format_prompt());

        // Notify finished
        callback(null);
    }

    async function completer(line, callback)
    {
        try
        {

            // Get the last argument
            let lastArgInfo = findLastArg(line);
            let lastArg = line.substring(lastArgInfo.lastArgPos);
    
            // If any special characters then can't match anything
            if (lastArg.match(/[\*\?\(\)\&\|\<\>]/))
                return [[], line];
    
            // Work out the glob pattern by closing any pending
            // opening quote and appending the wildcard character
            let glob = lastArg;
            if (lastArgInfo.pendingQuote)
                glob += lastArgInfo.pendingQuote;
            glob += "*";
    
            // Get matching files
            let matches = await ctx.layer.exec_ls(rwd, "ls -ald " + glob, true);
    
            function escape_match(name)
            {
                if (lastArgInfo.pendingQuote)
                    return `${lastArgInfo.pendingQuote}${name}${lastArgInfo.pendingQuote}`;
                
                return name.replace(/[\ \t\r\n\*\?\(\)\&\|\<\>]/g, "\\$&");
            }
    
            callback(null, [matches.map(x => escape_match(x.name)), lastArg]);
        }
        catch
        {
            callback(null, [[], ""]);
        }
    }
      
    // Start the repl
    repl = nodeRepl.start({ 
        prompt: format_prompt(), 
        eval: invoke_command,
        completer: completer,
    }); 

    // Handle Ctrl+C/Ctrl+D, '.exit',
    let exitResolve;
    repl.on('exit', () => {
        exitResolve();
    })

    // Don't return until the repl exits
    await new Promise((resolve, reject) => {exitResolve = resolve});
}

module.exports = {
    synopsis: "Opens an interactive command shell",
    spec: [
    ],
    run,
}
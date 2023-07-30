let nodeRepl = require('node:repl');
let chalk = require('chalk');

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
        return chalk.green(`${ping.model.name} (${vol})`) + " " + chalk.cyan(rwd) + "> ";
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
      
    // Start the repl
    repl = nodeRepl.start({ 
        prompt: format_prompt(), 
        eval: invoke_command 
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
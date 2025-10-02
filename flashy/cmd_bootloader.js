import path from 'node:path';
import fs from 'node:fs';

async function run(ctx)
{
    let cl = ctx.cl;

    // Print the bootloader path
    if (cl.print)
    {
        process.stdout.write(path.join(path.dirname(__filename), "bootloader_images"));
        return;
    }
    
    // Extract bootloader images
    let sourceDir = path.join(path.dirname(__filename), "bootloader_images") 
    let entries = fs.readdirSync(sourceDir, { withFileTypes: true });
    for (let entry of entries)
    {
        let sourcePath = path.join(sourceDir, entry.name);
        let destinationPath = path.join(cl.targetdir, entry.name);
        if (!entry.isDirectory())
        {
            console.log(`Copying to ${destinationPath}`);
            fs.copyFileSync(sourcePath, destinationPath);
        }
    };
}


export default {
    synopsis: "Extracts the packaged Flashy bootloader images to a directory, or prints the path to where the they can be found.",
    spec: [
        {
            name: "<targetdir>",
            default: ".",
            help: "Target directory to save the bootloader kernel images to. "
                + "Uses the current directory if not specified."
        },
        {
            name: "--print",
            help: "Prints the path to the bootloader images instead of extracting them."
        },
    ],
    usesSerialPort: false,
    run,
}
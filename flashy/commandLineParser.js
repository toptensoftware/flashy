let fs = require('fs');
let path = require('path');

// Parse a boolean value
function parse_bool(value)
{
    switch (value.toLowerCase())
    {
        case '': return true;           // eg: --switch with no :value
        case 'true': return true;
        case 'false': return false;
        case 'yes': return true;
        case 'no': return false;
        case '1': return true;
        case '0': return false;
    }
    throw new Error(`Invalid option value: '${value}'`);
}

// Parse an enumerated value
function parse_enum(values)
{
    values = values.map(x => x.toLowerCase());
    return function(value)
    {
        let index = values.indexOf(value.toLowerCase());
        if (index >= 0)
            return values[index];
        else
            throw new Error(`expected one of: ${values.map(x=>`'${x}'`).join(", ")}`);
    }
}

// Parse an integer value
function parse_integer(min, max)
{
    return function(value)
    {
        let intVal = parseInt(value);
        if (isNaN(intVal))
            throw new Error(`expected an integer value`);
        if (min !== undefined && intVal < min) 
            throw new Error(`value is less than minimum ${min}`);
        if (max !== undefined && intVal > max)
            throw new Error(`value is greater than maximum ${max}`);
        return intVal;
    }
}

// Create a parser based on value spec
function create_parser(valuespec)
{
    // Enumerated type eg: [apples|pears|bananas]
    if (valuespec.startsWith('[') && valuespec.endsWith(']'))
    {
        let values = valuespec.substring(1, valuespec.length - 1).split('|');
        return parse_enum(values);
    }

    // Integer?
    if (valuespec == "<n>")
    {
        return parse_integer();
    }
    return;
}


// Split names at | but allow for | in brackets
// eg: "-o|--option:[a|b|c]" => "-o", "--option:[a|b|c]"
function split_names(name)
{
    let names = [];
    let start = 0;
    let depth = 0;
    for (let i=0; i<name.length; i++)
    {
        switch (name[i])
        {
            case '|':
                if (depth == 0)
                {
                    names.push(name.substring(start, i));
                    start = i+1;
                }
                break;

            case '<':
            case '[':
            case '(':
                depth++;
                break;

            case '>':
            case ']':
            case ')':
                depth--;
                break;

            default:
                break;
        }
    }

    if (start < name.length)
        names.push(name.substring(start, name.length));
    return names;
}


// Prepare a command line spec
function prepare_spec(spec)
{
    let first_default_positional = false;
    let have_multi_positional = false;
    for (let s of spec)
    {
        // Parse name parts
        let names = split_names(s.name);
        s.optionNames = [];
        for (let n of names)
        {
            // Is it a named arg?
            if (n.startsWith("-"))
            {
                let colonPos = n.indexOf(':');
                if (colonPos > 0)
                {
                    // Create a validator for the value spec
                    if (s.parse === undefined)
                    {
                        s.parse = create_parser(n.substring(colonPos+1));
                    }

                    // Named value option
                    n = n.substring(0, colonPos)
                    s.type = "named";
                }
                else
                {
                    if (s.type == undefined)
                        s.type = "switch";
                }

                s.optionNames.push(n);
            }
            else if (n.startsWith("<"))
            {
                if (!n.endsWith(">"))
                    throw new Error("Bad arg spec");
                n = n.substring(1, n.length - 1);
                s.type = "positional";
            }
            else
            {
                s.type = "command";
            }


            // Setup a default camelCase key name if not set
            if (s.key === undefined)
            {
                // Remove leading dashes
                let nodashName = n;
                while (nodashName.startsWith('-'))
                    nodashName = nodashName.substring(1);

                // convert to camel case
                let parts = nodashName.split('-');
                parts = [parts[0].toLowerCase(), ...parts.slice(1).map(x => x[0].toUpperCase() + x.substring(1).toLowerCase())];
                s.key = parts.join("");
            }
        }

        // Is it multi-value
        if (s.multiValue === undefined)
        {
            if (Array.isArray(s.default))
                s.multiValue = true;
            else
                s.multiValue = false;
        }

        // Check positional constraints
        if (s.type == "positional")
        {
            if (first_default_positional && s.default === undefined)
                throw new Error(`Bad command line spec, required positional arg '${s.name}' can't come after positional arg with default value '${first_default_positional.name}'`);
            if (s.default !== undefined)
                first_default_positional = s;

            if (have_multi_positional)
                throw new Error(`Bad command line spec, multi-value positional arg '${s.name}' must be at end`);
            if (s.multiValue)
                have_multi_positional = true;
        }

        // Create default parser
        if (s.parse === undefined)
        {
            if (s.type == "switch")
            {
                s.parse = parse_bool;
            }
            else
            {
                s.parse = (value) => value;
            }
        }
    }
}

// Format a usage string from the spec
function format_usage(spec)
{
    let args = [];
    let commands = [];

    let hasOptions
    for (let s of spec)
    {
        switch (s.type)
        {
            case "command":
                commands.push(s.name);
                break;

            case "positional":
                if (s.default === undefined)
                {
                    args.push(`${s.name}${s.multiValue ? "..." : ""}`);
                }
                else
                {
                    args.push(`[${s.name}${s.multiValue ? "..." : ""}]`);
                }
                break;

            default:
                hasOptions = true;
                break;
        }
    }

    if (hasOptions)
    {
        args.push("[<options>]");
    }
    
    if (commands.length)
    {
        args = [...args, "<command>", "[<command_options>]"];
    }

    return args.join(" ");
}

// Word break a line
function break_line(str, max_width, indent)
{
    if (!indent)
        indent = '';
    // Does it fit?
    if (str.length <= max_width)
        return str;

    // Find the last space that does fit
    for (let i=max_width-1; i>=0; i--)
    {
        if (str[i] == ' ')
        {
            return str.substring(0, i) + "\n" + break_line(indent + str.substring(i+1), max_width, indent);
        }
    }

    // No break positions
    return str;
}

function format_aligned(items)
{
    // Work out width of first column
    let colWidth = 0;
    for (let i of items)
    {
        if (i[0].length > colWidth)
            colWidth = i[0].length;
    }
    if (colWidth > 40)
        colWidth = 40;

    // Format lines
    let lines = "";
    for (let i of items)
    {
        // Set with the first column text
        let str = i[0];

        // Wrap the second column text
        let help = i[1].split('\n').map(x => break_line(x, process.stdout.columns - 40, ''));

        // Format second column, wrapping over multiple lines if necessary
        let first = true;
        for (let h of help)
        {
            for (let hl of h.split("\n"))
            {
                // First column is wider than the column width, insert a line break
                if (str.length > colWidth)
                {
                    lines += "  " + str + "\n";
                    str = "";
                }

                // Format second column
                lines += "  " + str.padEnd(colWidth + (first ? 4 : 6), ' ') + hl + "\n";
                str = '';
                first = false;
            }
        }
    }
    
    return lines.trimEnd();
}

// Format help for command
function format_help(spec)
{
    let help = [];

    // Positional args first
    for (let s of spec)
    {
        if (!s.help || (s.type != "positional"))
            continue;

        help.push([
            `      ${s.name}${s.multiValue ? "..." : ""}`,
            s.help,
        ])
    }

    // Named args second
    for (let s of spec)
    {
        if (!s.help || (s.type=="positional" || s.type=="command"))
            continue;

        let helpNames = split_names(s.name);
        helpNames.sort((a,b) => (a.length - b.length));

        let str = "";
        if (helpNames[0].startsWith("--"))
            str += "      ";
        else 
            str += "  ";

        for (let i=0; i<helpNames.length; i++)
        {
            if(i > 0)
                str += ", ";
            str += helpNames[i];
            //if (s.type == "switch" && helpNames[i].startsWith("--"))
            //    str += "[:yes|no]";
        }

        help.push([
            str,
            s.help
        ]);
    }


    // Commands last
    let first = true;
    for (let s of spec)
    {
        if (!s.help || s.type!="command")
            continue;

        if (first)
        {
            help.push(["      <command>", ""])
            first = false;
        }

        help.push([
            `        ${s.name}`,
            s.help
        ])
    }

    return format_aligned(help).trimEnd();
}

// Parse command line
function parse(args, options)
{
    let spec = options.spec;

    // Expand single character args eg: -abcd:value => -a -b -c -d:value
    for (let i=0; i<args.length; i++)
    {
        let arg = args[i];

        let m = arg.match(/^-([^-:=]+)([:=]?)(.*)$/);
        if (m)
        {
            let expanded = [];
            for (let j=0; j<m[1].length; j++)
            {
                if (j < m[1].length - 1 || !m[2])
                {
                    expanded.push(`-${m[1][j]}`);
                }
                else
                {
                    expanded.push(`-${m[1][j]}${m[2]}${m[3]}`);
                }
            }
            args.splice(i, 1, ...expanded);
            i += expanded.length - 1;
        }
    }

    // Setup result with defaults
    let result = {};
    for (let s of spec)
    {
        // Setup default value
        if (s.default !== undefined)
        {
            if (Array.isArray(s.default))
            {
                // Copy array
                result[s.key] = [...s.default];
            }
            else
            {
                // Store value
                result[s.key] = s.default;
            }
        }
    }

    // Separate into different arg types
    let positional_specs = spec.filter(x => x.type == "positional");
    let command_specs = spec.filter(x => x.type == "command");
    let named_specs = spec.filter(x => x.type == "named" || x.type == "switch");
    let pattern_specs = spec.filter(x => x.valuePattern !== undefined);

    // Process args
    let position = 0;
    for (let i=0; i<args.length; i++)
    {
        // Get the arg
        let arg = args[i];

        // End marker?
        if (arg == "--")
        {
            result.$tail = args.slice(i+1);
            return result;
        }

        // Named option?
        let arg_spec;
        let arg_value;
        let m = arg.match(/^(--|-)([^:=]+)([:=]?)(.*)$/);
        if (m)
        {
            // Find it's arg spec
            arg_spec = named_specs.find(x => x.optionNames.indexOf((m[1]+m[2])) >= 0);
            if (!arg_spec)
            {
                if (options.failOnUnknownArg === false)
                {
                    result.$tail = args.slice(i);
                    return result;
                }
                throw new Error(`Unknown option '${arg}'`);
            }

            if (!m[3])
            {
                // If there's a "when present" default, use it
                if (arg_spec.defaultWhenPresent!== undefined)
                    arg_value = arg_spec.defaultWhenPresent;

                // Otherwise if it's a named option and no value is specified by ':' or 
                // '=' then read the value from the next command line arg
                else if (arg_spec.type == 'named' && i + 1 < args.length)
                {
                    arg_value = args[i+1];
                    i++;
                }
                else
                {
                    arg_value = "";
                }
            } 
            else
                arg_value = m[4];
        }
        else
        {
            // Look for named spec with value pattern
            let pattern_spec = pattern_specs.find(x => arg.match(x.valuePattern));
            if (pattern_spec)
            {
                arg_spec = pattern_spec;
                arg_value = arg;
            }
            else
            {
                // Look for matching command
                let cmd_spec = command_specs.find(x => x.name == arg);
                if (cmd_spec)
                {
                    // It's a command
                    result.$command = cmd_spec.name;
                    result.$tail = args.slice(i+1);
                    return result;
                }
                else
                {
                    // It's a positional value
    
                    // Too many?
                    if (position >= positional_specs.length)
                    {
                        result.$tail = args.slice(i);
                        return result;
                    }
        
                    // Use next positional arg
                    arg_spec = positional_specs[position];
                    if (!arg_spec.multiValue)
                        position++;
                    arg_value = arg;
                }
            }
        }

        // Parse value
        try
        {
            arg_value = arg_spec.parse(arg_value);
        }
        catch (err)
        {
            throw new Error(`invalid value '${arg}': ${err.message}`);
        }

        // Store it
        if (arg_spec.multiValue)
        {
            if (result[arg_spec.key] === undefined)
                result[arg_spec.key] = [ arg_value ];
            else
                result[arg_spec.key].push(arg_value);
        }
        else
            result[arg_spec.key] = arg_value;

        // Stop processing if terminal
        if (arg_spec.terminal)
        {
            result.$tail = args.slice(i+1);
            return result;
        }
    }

    // Done
    return result;
}

// Check all required args have been specified
function check(spec, result)
{
    // Make sure all required args have been supplied
    for (let s of spec)
    {
        // Ignore if has a default
        if (s.default)
            continue;
        if (s.type == "named" || s.type == "positional")
        {
            // Make sure it was set
            if (result[s.key] === undefined)
            {
                throw new Error(`Missing argument '${s.name}' (${s.help})`);
            }
        }

    }

    // Make sure nothing left unprocessed
    if (spec.$tail && spec.$tail.length > 0)
    {
        throw new Error(`Invalid arguments, don't know what to do with: '${spec.$tail.join(' ' )}'`);
    }
}

// Helper to show version info from package file 
function show_version(options)
{
    // Load package
    let pkg = JSON.parse(fs.readFileSync(path.join(options.packageDir, 'package.json')), "utf8");

    console.log(`${pkg.name} ${pkg.version} - ${pkg.description}`)
    if (options.copyright)
    {
        console.log(options.copyright);
    }
    else if (pkg.copyright)
    {
        console.log(`${pkg.copyright}`);
    }
    else if (pkg.author)
    {
        console.log(`Copyright (C) ${new Date().getFullYear()} ${pkg.author}`);
    }
}

// Helper to show version and help info
function show_help(spec, options)
{
    // Show version
    if (options.packageDir)
    {
        show_version(options);
        console.log("");
    }

    if (options.synopsis)
    {
        console.log(break_line(options.synopsis, process.stdout.columns, ''));
        console.log("");
    }

    // Output usage line
    let usagePrefix = options.usagePrefix || "";
    if (usagePrefix.length)
        usagePrefix += ' ';
    usagePrefix = ` Usage: ${usagePrefix}`;
    console.log(break_line(`${usagePrefix}${format_usage(spec)}`, process.stdout.columns, "".padEnd(usagePrefix.length, ' '), ''));
    console.log("");

    // Output help
    console.log(" Options:");
    console.log(format_help(spec));
}

// Helper to show version or help if user asked for it
function handle_help(spec, cl, options)
{
    if (cl.version)
    {
        show_version(options);
        process.exit(0);
    }
    
    if (cl.help)
    {
        show_help(spec, options);
        process.exit(0);
    }
}

// Create a parser
function parser(options)
{
    // Prepare supplied spec
    prepare_spec(options.spec);

    // Return API
    return {
        parse: (args) => parse(args, options),
        check: (cl) => check(options.spec, cl),
        format_usage: () => format_usage(options.spec),
        format_help: () => format_help(options.spec),
        show_version: () => show_version(options),
        show_help: () => show_help(options),
        handle_help: (cl) => handle_help(options.spec, cl, options),
    }
}

module.exports = {
    parser,
    parse_bool,
    parse_enum,
    parse_integer,
}
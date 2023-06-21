
function piModelFromRevision(revision)
{
    if (revision & (1 << 23))
    {
        // new revision
        switch ((revision >> 4) & 0xFF)
        {
            case 0: return { name: "Raspberry Pi Model A", major: 1, ledPin: -16 };
            case 1: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 2: return { name: "Raspberry Pi Model A+", major: 1, ledPin: 47 };
            case 3: return { name: "Raspberry Pi Model B+", major: 1, ledPin: 47 };
            case 4: return { name: "Raspberry Pi 2 Model B", major: 2, ledPin: 47 };
            case 6: return { name: "Compute Module", major: 1, ledPin: 47 };
            case 8: return { name: "Raspberry Pi 3 Model B", major: 3, ledPin: 0 };
            case 9: return { name: "Raspberry Pi Zero", major: 1, ledPin: -47 };
            case 10: return { name: "Compute Module 3", major: 3, ledPin: 0 };
            case 12: return { name: "Raspberry Pi Zero W", major: 1, ledPin: -47 };
            case 13: return { name: "Raspberry Pi 3 Model B+", major: 3, ledPin: 29 };
            case 14: return { name: "Raspberry Pi 3 Model A+", major: 3, ledPin: 29 };
            case 16: return { name: "Compute Module 3+", major: 3, ledPin: 0 };
            case 17: return { name: "Raspberry Pi 4 Model B", major: 4, ledPin: 42 };
            case 18: return { name: "Raspberry Pi Zero 2 W", major: 3, ledPin: -29 };
            case 19: return { name: "Raspberry Pi 400", major: 4, ledPin: 42 };
            case 20: return { name: "Compute Module 4", major: 4, ledPin: 42 };
            case 21: return { name: "Compute Module 4S", major: 4, ledPin: 0 };
        }
    }
    else
    {
        switch (revision)
        {
            case 0x02: return { name: "Raspberry Pi Model B R1", major: 1, ledPin: -16 };
            case 0x03: return { name: "Raspberry Pi Model B R1", major: 1, ledPin: -16 };
            case 0x04: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x05: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x06: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x07: return { name: "Raspberry Pi Model A", major: 1, ledPin: -16 };
            case 0x08: return { name: "Raspberry Pi Model A", major: 1, ledPin: -16 };
            case 0x09: return { name: "Raspberry Pi Model A", major: 1, ledPin: -16 };
            case 0x0D: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x0E: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x0F: return { name: "Raspberry Pi Model B R2", major: 1, ledPin: -16 };
            case 0x10: return { name: "Raspberry Pi Model B+", major: 1, ledPin: 47 };
            case 0x11: return { name: "Compute Module", major: 1, ledPin: 47 };
            case 0x12: return { name: "Raspberry Pi Model A+", major: 1, ledPin: 47 };
            case 0x13: return { name: "Raspberry Pi Model B+", major: 1, ledPin: 47 };
            case 0x14: return { name: "Compute Module", major: 1, ledPin: 47 };
            case 0x15: return { name: "Raspberry Pi Model A+", major: 1, ledPin: 47 };
        }
    }

    return {
        name: "Unknown",
        major: 0,
        ledPin: 0
    }
}

module.exports = {
    piModelFromRevision,
};
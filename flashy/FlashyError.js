class FlashyError extends Error
{
    constructor(message)
    {
        super(message);
    }
}

module.exports = FlashyError;
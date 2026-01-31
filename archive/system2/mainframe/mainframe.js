const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const BOX_DIR = path.join(__dirname, "mailbox");
const INBOX = path.join(BOX_DIR, "inbox");
const OUTBOX = path.join(BOX_DIR, "outbox");
const ARCHIVE = path.join(BOX_DIR, "archive");

console.log(`[MAINFRAME] Online. Watching ${INBOX} for punch cards...`);

// Simple polling watcher to avoid double-events from fs.watch
setInterval(() => {
    fs.readdir(INBOX, (err, files) => {
        if (err) return console.error(`[SYSTEM ERROR] Cannot read inbox: ${err.message}`);

        files.forEach(file => {
            if (!file.endsWith(".js")) return;

            const jobPath = path.join(INBOX, file);
            const archivePath = path.join(ARCHIVE, file);
            const logPath = path.join(OUTBOX, file + ".log");

            console.log(`[PROCESSING] Consuming card: ${file}`);

            // Move to archive first to prevent double-processing during execution
            // (In a real mainframe, the card reader physically takes the card)
            try {
                fs.renameSync(jobPath, archivePath);
            } catch (e) {
                console.error(`[ERROR] Jammed card reader (move failed): ${e.message}`);
                return;
            }

            // Execute the card
            exec(`node "${archivePath}"`, (error, stdout, stderr) => {
                const timestamp = new Date().toISOString();
                let logContent = `--- JOB REPORT: ${file} ---\n`;
                logContent += `Timestamp: ${timestamp}\n`;
                logContent += `Status: ${error ? "CRASHED (Exit Code " + error.code + ")" : "SUCCESS"}\n`;
                logContent += `\n--- STDOUT ---\n${stdout || "(no output)"}\n`;
                logContent += `\n--- STDERR ---\n${stderr || "(no errors)"}\n`;

                fs.writeFile(logPath, logContent, (err) => {
                    if (err) console.error(`[ERROR] Printer jammed (log write failed): ${err.message}`);
                    else console.log(`[COMPLETE] Output printed to ${logPath}`);
                });
            });
        });
    });
}, 2000); // Check every 2 seconds

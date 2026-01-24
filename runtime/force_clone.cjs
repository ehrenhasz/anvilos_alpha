const fs = require('fs');
const { execSync } = require('child_process');

const FILE = 'card_queue.json';
let queue = [];

try {
    queue = JSON.parse(fs.readFileSync(FILE, 'utf8'));
} catch (e) {
    console.error("Failed to read queue:", e);
    process.exit(1);
}

let processed = 0;
let skipped = 0;
let errors = 0;

queue.forEach(card => {
        // Look for OSS Clone tasks regardless of status (force verify)
        if (card.description.startsWith('OSS: Clone')) {
            // Pattern: "OSS: Clone <URL> into <DIR>, then sever git ties (rm -rf .git)."
            const match = card.description.match(/Clone\s+(.*?)\s+into\s+(.*?),\s+then/);
        
        if (match) {
            const url = match[1];
            const dir = match[2];
            
            console.log(`[${card.id}] Processing: ${dir}`);
            
            try {
                // Ensure parent dir exists
                execSync(`mkdir -p ${dir}`);
                
                // Check if directory is effectively empty (only metadata.json)
                const files = fs.readdirSync(dir);
                if (files.length > 1 && files.includes('.git')) {
                     // If .git exists, we might need to remove it? 
                     // Or if it's already full code?
                     console.log(`  Directory has content. Checking for .git...`);
                }
                
                // If .git is NOT present but other files are, it might be done already.
                // If .git IS present, we need to remove it.
                // If only metadata.json, we clone.
                
                if (files.length <= 1 || (files.length === 1 && files[0] === 'metadata.json')) {
                    // Temporarily move metadata.json if it exists
                    const hasMetadata = files.includes('metadata.json');
                    if (hasMetadata) {
                        execSync(`mv ${dir}/metadata.json ${dir}_metadata_backup.json`);
                        // Ensure dir is empty/removed so clone works? 
                        // Actually git clone expects the DIR to not exist or be empty.
                        // If dir exists and is empty, it works.
                        // If dir doesn't exist, it creates it.
                        // After mv, dir might be empty.
                    }

                    console.log(`  Cloning ${url}...`);
                    try {
                        execSync(`git clone ${url} ${dir} --quiet`, { stdio: 'inherit' });
                    } catch (err) {
                        // If clone fails, restore metadata
                         if (hasMetadata) {
                            execSync(`mv ${dir}_metadata_backup.json ${dir}/metadata.json`);
                        }
                        throw err;
                    }
                    
                    if (hasMetadata) {
                        // If directory was recreated by clone (it wasn't, we cloned INTO it? No git clone TARGET creates target)
                        // If target existed and was empty, git clone uses it.
                        // We need to move metadata back INTO dir.
                        execSync(`mv ${dir}_metadata_backup.json ${dir}/metadata.json`);
                    }

                    console.log(`  Severing git ties...`);
                    execSync(`rm -rf ${dir}/.git`);
                    
                    card.status = 'done';
                    card.result = "Manually executed via force_clone.cjs";
                    processed++;
                } else {
                    console.log(`  Skipping clone (directory not empty). Checking for lingering .git...`);
                    if (fs.existsSync(`${dir}/.git`)) {
                        execSync(`rm -rf ${dir}/.git`);
                        console.log(`  Removed lingering .git.`);
                    }
                    card.status = 'done';
                    card.result = "Skipped clone (already populated). Ensured .git removed.";
                    skipped++;
                }
                
            } catch (e) {
                console.error(`  FAILED: ${e.message}`);
                // Don't fail the script, just the card
                card.status = 'error';
                card.result = `Force clone failed: ${e.message}`;
                errors++;
            }
        }
    }
});

fs.writeFileSync(FILE, JSON.stringify(queue, null, 2));
console.log(`\nOperation Complete.`);
console.log(`Processed: ${processed}`);
console.log(`Skipped/Verified: ${skipped}`);
console.log(`Errors: ${errors}`);

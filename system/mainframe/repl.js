const { GoogleGenerativeAI } = require("@google/generative-ai");

// 1. Setup the "System" (Your API Key)
const genAI = new GoogleGenerativeAI("YOUR_API_KEY_HERE");

// 2. Select your "Sub-Module" (The Gemini Model)
// This is where you swap in "gemini-1.5-pro", "gemini-1.5-flash", etc.
const model = genAI.getGenerativeModel({ 
    model: "gemini-1.5-pro",
    systemInstruction: "You are a coding specialist. Answer only in JSON." // Optional raw instructions
});

async function runRaw() {
  // 3. Your "Raw Instruction" Input
  const prompt = "Write a Node.js script to list files in a directory.";

  console.log("... Generative AI processing ...");
  
  // 4. The Output
  const result = await model.generateContent(prompt);
  const response = await result.response;
  const text = response.text();
  
  console.log(text);
}

runRaw();

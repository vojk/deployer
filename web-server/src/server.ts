import app from "./app";

const PORT = parseInt(process.env.PORT ?? "3000", 10);

app.listen(PORT, () => {
  console.log(`Deploy server listening on port ${PORT}`);
});

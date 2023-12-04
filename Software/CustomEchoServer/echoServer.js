const http = require("http");
const port = 8080;

http
  .createServer((req, res) => {
    const headers = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "*",
      "Access-Control-Allow-Headers": "*",
    };

    res.writeHead(200, headers);
    let body = [];
    req
      .on("data", (chunk) => {
        body.push(chunk);
      })
      .on("end", () => {
        body = body.toString();
        console.log(
          `==== Received a ${req.method} to path ${req.url} with the following payload ====`
        );

        console.log(body);

        res.statusCode = 200;
        res.end();
      });
  })
  .listen(port);
console.log("listening on port " + port);

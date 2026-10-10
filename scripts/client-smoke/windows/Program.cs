using System.Text.Json;
using Helicoid.Domain.Client;
using Helicoid.Domain.Model;
var input = JsonDocument.Parse(await Console.In.ReadToEndAsync()).RootElement;
string Read(string key) => input.GetProperty(key).GetString()!;
using var client = DevelopmentServerClient.CreateLoopback(Read("address"));
var health = await client.GetHealthAsync(CancellationToken.None);
if (health.Features!["remote_control"] || !health.BootstrapCompleted) throw new Exception("unexpected readiness");
var login = await client.LoginAsync(Read("username"), Read("password"), Read("manager"), CancellationToken.None);
if (!client.HasSession || login.Account!.Username != Read("username")) throw new Exception("login failed");
var session = await client.GetSessionAsync(CancellationToken.None);
if (session.Account!.Id != login.Account.Id) throw new Exception("session identity mismatch");
var inventory = await client.GetInventoryAsync(CancellationToken.None);
if (inventory.Count != 1 || inventory[0].Id.ToString() != Read("computer_id") || inventory[0].Status != ComputerStatus.PendingApproval) throw new Exception("inventory mismatch");
try { await client.LoginAsync(Read("username"), "incorrect-smoke-password", Read("manager"), CancellationToken.None); throw new Exception("bad credentials accepted"); }
catch (ServerClientException error) when (error.Failure == ServerClientFailure.Unauthorized) {}
if (client.HasSession) throw new Exception("old bearer survived failed login");
await client.LoginAsync(Read("username"), Read("password"), Read("manager"), CancellationToken.None);
await client.LogoutAsync(CancellationToken.None);
if (client.HasSession) throw new Exception("logout retained bearer");
Console.WriteLine("Windows client: real health/login/session/scoped inventory/denial/logout PASS");

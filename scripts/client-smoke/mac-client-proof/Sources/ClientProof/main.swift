import Foundation
import HelicoidCore
struct Input: Decodable { let address: String; let username: String; let password: String; let manager: String; let computer_id: String }
@main struct Proof {
 static func main() async throws {
  let input = try JSONDecoder().decode(Input.self, from: FileHandle.standardInput.readDataToEndOfFile())
  let client = LoopbackDevelopmentClient(configuration: .init(baseURL: URL(string: input.address)!, managerPublicIdentity: input.manager))
  _ = try await client.connect(address: input.address).get()
  let account = try await client.signIn(username: input.username, password: input.password).get()
  guard !account.rawValue.isEmpty else { throw ProofError.failed }
  let state = try await client.managerApprovalState().get()
  guard state == .approved else { throw ProofError.failed }
  let inventory = try await client.authorizedInventory().get()
  guard inventory.count == 1, inventory[0].id.rawValue == input.computer_id, !inventory[0].isOnline, inventory[0].enrollmentState == "pending" else { throw ProofError.failed }
  if case .success = await client.signIn(username: input.username, password: "incorrect-smoke-password") { throw ProofError.failed }
  if case .success = await client.authorizedInventory() { throw ProofError.failed }
  _ = try await client.signIn(username: input.username, password: input.password).get()
  await client.signOut()
  if case .success = await client.authorizedInventory() { throw ProofError.failed }
  print("Mac client: real health/login/session/scoped inventory/denial/logout PASS")
 }
 enum ProofError: Error { case failed }
}

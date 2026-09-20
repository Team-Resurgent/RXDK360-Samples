#region File Information
//-----------------------------------------------------------------------------
// Scenarios.cs
//
// The profile automation scenarios demonstrated in this sample.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.Linq;
using Microsoft.Test.Xbox.Profiles;
using XDevkit;

namespace Microsoft.ATG.ProfileAutomation
{
    /// <summary>
    /// The profile automation scenarios demonstrated in this sample.
    /// </summary>
    static class Scenarios
    {
        /// <summary>
        /// This scenario involves creating a new LIVE profile with a gold subscription
        /// and signing in the profile after it is created.
        /// Also shown is obtaining the user index and sign-in state for a profile.
        /// </summary>
        public static void CreateAndSignInGoldConsoleProfile()
        {
            // Use the default console for this sample.
            // NOTE: You may specify any alternate console by name or IP address.
            XboxManagerClass xboxManager = new XboxManagerClass();
            IXboxConsole console = xboxManager.OpenConsole(xboxManager.DefaultConsole);
            
            // This object provides access to profile management on the console.
            ConsoleProfilesManager profilesManager = console.CreateConsoleProfilesManager();

            // Sign out all profiles for this scenario.
            profilesManager.SignOutAllUsers();

            // Create the new profile.
            Console.WriteLine("Creating a new LIVE gold-tier profile on console {0}.", console.Name);
            Console.WriteLine("NOTE: Profile creation may take several minutes to complete.");

            ConsoleProfile goldProfile = profilesManager.CreateConsoleProfile(true);
            Console.WriteLine("Profile {0} was successfully created.", goldProfile);

            // Sign in the new profile.
            Console.WriteLine("Signing in profile {0}.", goldProfile);
            goldProfile.SignIn(UserIndex.Zero);

            // Report the user index and sign-in state of the profile.
            Console.WriteLine("User index for profile {0} is {1}.", goldProfile, goldProfile.GetUserIndex());
            Console.WriteLine("Sign-in state for profile {0} is {1}.", goldProfile, goldProfile.GetUserSigninState());

            Console.WriteLine("Press any key to continue...");
            Console.ReadKey(true);

            // Sign out the new profile.
            Console.WriteLine("Signing out profile {0}.", goldProfile);
            goldProfile.SignOut();

            // Report the sign-in state of the profile.
            Console.WriteLine("Sign-in state for profile {0} is {1}.", goldProfile, goldProfile.GetUserSigninState());

            // Delete new profile, if requested.
            Console.WriteLine("Scenario is complete.  Do you want to delete the profile? (Y/N)");
            ConsoleKeyInfo keyInfo = Console.ReadKey(true);
            if (keyInfo.Key == ConsoleKey.Y)
            {
                Console.WriteLine("Deleting profile {0}.", goldProfile);
                profilesManager.DeleteConsoleProfile(goldProfile);
            }
        }

        /// <summary>
        /// This scenario involves enumerating profiles on the console
        /// and signing in an existing profile.
        /// </summary>
        public static void EnumerateAndSignInAnExistingConsoleProfile()
        {
            // Use the default console for this sample.
            // NOTE: You may specify any alternate console by name or IP address.
            XboxManagerClass xboxManager = new XboxManagerClass();
            IXboxConsole console = xboxManager.OpenConsole(xboxManager.DefaultConsole);

            // This object provides access to profile management on the console.
            ConsoleProfilesManager profilesManager = console.CreateConsoleProfilesManager();

            // Sign out all profiles for this scenario.
            profilesManager.SignOutAllUsers();

            // Enumerate profiles.
            Console.WriteLine("Enumerating profiles on console {0}.", console.Name);
            IEnumerable<ConsoleProfile> profiles = profilesManager.EnumerateConsoleProfiles();
            if (!profiles.Any())
            {
                // No profiles exist.
                Console.WriteLine("No profiles were found.");
                return;
            }
            else
            {
                // Display profiles on console.
                Console.WriteLine("Profiles found:");
                foreach (var profile in profiles)
                {
                    Console.WriteLine("\t{0}", profile);
                }
            }

            // Display details about the first available profile.
            ConsoleProfile firstProfile = profiles.First();
            Console.WriteLine("First profile found is {0}.", firstProfile);
            Console.WriteLine("Subscription tier for profile {0} is {1}.", firstProfile, firstProfile.Tier);

            // Sign in the profile.
            Console.WriteLine("Signing in profile {0}.", firstProfile);
            firstProfile.SignIn(UserIndex.Zero);
            
            // Report the user index and sign-in state of the profile.
            Console.WriteLine("User index for profile {0} is {1}.", firstProfile, firstProfile.GetUserIndex());
            Console.WriteLine("Sign-in state for profile {0} is {1}.", firstProfile, firstProfile.GetUserSigninState());

            Console.WriteLine("Press any key to continue...");
            Console.ReadKey(true);

            // Sign out the new profile.
            Console.WriteLine("Signing out profile {0}.", firstProfile);
            firstProfile.SignOut();
        }

        /// <summary>
        /// This scenario involves creating two LIVE profiles, sending a friend request
        /// from one profile to the other, and accepting the friend request.
        /// </summary>
        /// <remarks>
        /// NOTE: While this example only uses a single console, the two profiles may
        /// be on different consoles when sending or accepting the friend request.
        /// </remarks>
        public static void CreateTwoLiveProfilesAndMakeThemFriends()
        {
            // Use the default console for this sample.
            // NOTE: You may specify any alternate console by name or IP address.
            XboxManagerClass xboxManager = new XboxManagerClass();
            IXboxConsole console = xboxManager.OpenConsole(xboxManager.DefaultConsole);

            // This object provides access to profile management on the console.
            ConsoleProfilesManager profilesManager = console.CreateConsoleProfilesManager();

            // Sign out all profiles for this scenario.
            profilesManager.SignOutAllUsers();

            // Create the new profiles.
            Console.WriteLine("Creating two new LIVE gold-tier profiles on console {0}.", console.Name);
            Console.WriteLine("NOTE: Profile creation may take several minutes to complete.");

            ConsoleProfile profileA = profilesManager.CreateConsoleProfile(true);
            Console.WriteLine("Profile {0} was successfully created.", profileA);

            ConsoleProfile profileB = profilesManager.CreateConsoleProfile(true);
            Console.WriteLine("Profile {0} was successfully created.", profileB);

            // Sign in both profiles.
            Console.WriteLine("Signing in profiles {0} and {1}.", profileA, profileB);
            profileA.SignIn(UserIndex.Zero);
            profileB.SignIn(UserIndex.One);

            // Send the friend request.
            Console.WriteLine("Profile {0} sends friend request to profile {1}.", profileA, profileB);
            profileA.Friends.SendFriendRequest(profileB);

            Console.WriteLine("Press any key to continue...");
            Console.ReadKey(true);

            // Accept the friend request.
            Console.WriteLine("Profile {0} accepts friend request from profile {1}.", profileB, profileA);
            profileB.Friends.AcceptFriendRequest(profileA);

            // Enumerate friends and display results.
            Console.WriteLine("First friend of profile {0} is {1}.", profileA,
                profileA.Friends.EnumerateFriends().First());
            Console.WriteLine("First friend of profile {0} is {1}.", profileB,
                profileB.Friends.EnumerateFriends().First());

            // Delete new profiles, if requested.
            Console.WriteLine("Scenario is complete.  Do you want to delete the profiles? (Y/N)");
            ConsoleKeyInfo keyInfo = Console.ReadKey(true);
            if (keyInfo.Key == ConsoleKey.Y)
            {
                Console.WriteLine("Deleting profiles {0} and {1}.", profileA, profileB);
                profilesManager.DeleteConsoleProfile(profileA);
                profilesManager.DeleteConsoleProfile(profileB);
            }
        }


        /// <summary>
        /// This scenario involves creating two LIVE profiles, starting a new party,
        /// and adding both profiles to the party.
        /// </summary>
        public static void CreateTwoLiveProfilesAndAddThemToAParty()
        {
            // Use the default console for this sample.
            // NOTE: You may specify any alternate console by name or IP address.
            XboxManagerClass xboxManager = new XboxManagerClass();
            IXboxConsole console = xboxManager.OpenConsole(xboxManager.DefaultConsole);

            // This object provides access to profile management on the console.
            ConsoleProfilesManager profilesManager = console.CreateConsoleProfilesManager();

            // This object provides access to party management on the console
            PartyManager partyManager = console.CreatePartyManager();

            // Sign out all profiles for this scenario.
            profilesManager.SignOutAllUsers();

            // Create the new profiles.
            Console.WriteLine("Creating two new LIVE gold-tier profiles on console {0}.", console.Name);
            Console.WriteLine("NOTE: Profile creation may take several minutes to complete.");

            ConsoleProfile profileA = profilesManager.CreateConsoleProfile(true);
            Console.WriteLine("Profile {0} was successfully created.", profileA);

            ConsoleProfile profileB = profilesManager.CreateConsoleProfile(true);
            Console.WriteLine("Profile {0} was successfully created.", profileB);

            // Sign in the first profile.
            Console.WriteLine("Signing in profile {0}.", profileA);
            profileA.SignIn(UserIndex.Zero);

            // Create the party.
            Console.WriteLine("Starting a party with profile {0} as the leader.", profileA);
            partyManager.CreateParty(profileA);

            // List the party members.  (Only the leader is in the party.)
            Console.WriteLine("Current Party members are:");
            foreach (var member in partyManager.GetPartyMembers())
            {
                Console.WriteLine("\t{0}", member);
            }

            Console.WriteLine("Press any key to continue...");
            Console.ReadKey(true);

            // Sign in the second profile.
            Console.WriteLine("Signing in profile {0}.", profileB);
            profileB.SignIn(UserIndex.One);

            // Add the second profile to the party.
            // NOTE: Since this profile is local to the console, AddLocalUserToParty is
            //       used rather than JoinParty.
            Console.WriteLine("Adding profile {0} to the party.", profileB);
            partyManager.AddLocalUserToParty(profileB);

            // List the party members.
            Console.WriteLine("Current Party Members are:");
            foreach (var member in partyManager.GetPartyMembers())
            {
                Console.WriteLine("\t{0}", member);
            }

            Console.WriteLine("Press any key to continue...");
            Console.ReadKey(true);

            // End the party.
            Console.WriteLine("Ending the party.");
            partyManager.LeaveParty();

            // Delete new profiles, if requested.
            Console.WriteLine("Scenario is complete.  Do you want to delete the profiles? (Y/N)");
            ConsoleKeyInfo keyInfo = Console.ReadKey(true);
            if (keyInfo.Key == ConsoleKey.Y)
            {
                Console.WriteLine("Deleting profiles {0} and {1}.", profileA, profileB);
                profilesManager.DeleteConsoleProfile(profileA);
                profilesManager.DeleteConsoleProfile(profileB);
            }
        }
    }
}

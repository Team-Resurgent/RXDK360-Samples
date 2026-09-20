#region File Information
//-----------------------------------------------------------------------------
// Program.cs
//
// Program entry point.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace Microsoft.ATG.ProfileAutomation
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        static void Main(string[] args)
        {
            Console.WriteLine("ProfileAutomation Sample");
            Console.WriteLine("------------------------");

            List<Scenario> scenarios = GetAllScenarios();
            for (; ; )
            {
                Scenario scenario = PresentMenu(scenarios);
                if (scenario == null)
                    break;

                try
                {
                    Console.WriteLine();
                    scenario.Invoke();
                }
                catch (Exception ex)
                {
                    Console.WriteLine("The following error occurred during the scenario:");
                    Console.WriteLine(ex);
                }
            }
        }

        #region Menu System Helpers

        /// <summary>
        /// Helper class to encapsulate a single scenario.
        /// </summary>
        private class Scenario
        {
            private MethodInfo methodInfo;
            
            public Scenario(MethodInfo methodInfo)
            {
                this.methodInfo = methodInfo;
            }
            
            public override string ToString()
            {
                return methodInfo.Name;
            }
            
            public void Invoke()
            {
                methodInfo.Invoke(null, null);
            }
        }

        /// <summary>
        /// Use Reflection to enumerate scenarios in this sample.
        /// </summary>
        private static List<Scenario> GetAllScenarios()
        {
            Type scenariosType = typeof(Scenarios);
            var methods = scenariosType.GetMethods(BindingFlags.Static | BindingFlags.Public);
            var scenarios = from method in methods
                            where !method.GetParameters().Any()
                            orderby method.Name
                            select new Scenario(method);
            return scenarios.ToList();
        }

        /// <summary>
        /// Display the specified list and allow the user to make a selection.
        /// </summary>
        private static T PresentMenu<T>(List<T> items)
        {
            Console.WriteLine();
            for (int i = 0; i < items.Count; ++i)
                Console.WriteLine("{0}. {1}", (i + 1), items[i]);

            for (; ; )
            {
                Console.Write("Select an item (or 0 to exit): ");
                int selection;
                if (int.TryParse(Console.ReadLine(), out selection))
                {
                    if (selection == 0)
                        return default(T);

                    if ((selection > 0) && (selection <= items.Count))
                        return items[selection - 1];
                }
            }
        }

        #endregion
    }
}
